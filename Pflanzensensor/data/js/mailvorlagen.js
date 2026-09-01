/**
 * @file mailvorlagen.js
 * @brief Prüfung und Vorschau für die Mailvorlagen
 *
 * Die Ersetzung gibt es zweimal: hier für die Vorschau und in
 * Pflanzensensor/src/utils/mail_template.h für den Versand. Damit beide sich
 * einig bleiben, tragen test/js/mailvorlagen.test.mjs und
 * test/test_mail_template/test_mail_template.cpp dieselbe durchnummerierte
 * Falltabelle - läuft eine Seite aus dem Tritt, fällt es dort auf und nicht
 * erst beim Empfänger der Mail.
 *
 * Beim Entwickeln hart neu laden: statische Dateien gehen mit max-age=86400 raus.
 */

const ZEILE_MAX = 256;
const BETREFF_MAX = 120;
const RUMPF_MAX = 1600;
const STIL_MAX = 1200;

const BLOCK_MESSWERTE = 'messwerte';
const BLOCK_AUFFAELLIGE = 'auffaellige';
const BEKANNT = ['geraet', 'name', 'ip', 'ssid', 'neustarts', 'laufzeit', 'datum', 'uhrzeit',
                 BLOCK_MESSWERTE, BLOCK_AUFFAELLIGE];

/** Beispielwerte für die Vorschau - dieselbe Auswahl wie am Gerät. */
const BEISPIEL = {
  geraet: 'Frameclaw PS',
  name: 'frameclaw-ps.local',
  ip: '192.168.1.44',
  ssid: 'Magrathea',
  neustarts: '7',
  laufzeit: '2d 5h 13m',
  datum: '31.08.2026',
  uhrzeit: '18:24'
};

/**
 * Die Tabelle erzeugt das Gerät, nicht die Vorlage - deshalb steht hier fertiges
 * HTML und nicht Vorlagentext. Spiegel von gibMesswerte() in mail_vorlagen.cpp.
 */
const BEISPIEL_BLOCK = {
  messwerte: [
    '<table class="werte">',
    '<tr><td class="name">🟢 Bodenfeuchte</td><td class="wert">42.0%</td></tr>',
    '<tr><td class="name">🟡 Lichtstärke</td><td class="wert">12.5%</td></tr>',
    '<tr><td class="name">🟢 Lufttemperatur</td><td class="wert">22.4°C</td></tr>',
    '</table>'
  ],
  auffaellige: [
    '<table class="werte">',
    '<tr><td class="name">🟡 Lichtstärke</td><td class="wert">12.5%</td></tr>',
    '</table>'
  ]
};

/**
 * Länge in UTF-8-Bytes.
 *
 * Von Hand gezählt statt mit TextEncoder: die Testsandbox lädt diese Datei in
 * einen vorbereiteten Kontext, und je weniger Globals sie braucht, desto
 * ehrlicher ist der Test. Wichtig ist die Byte- und nicht die Zeichenzahl -
 * das Attribut maxlength zählt UTF-16-Einheiten, ein Emoji wäre dort 2 statt 4.
 */
function byteLaenge(text) {
  let n = 0;
  for (const zeichen of String(text || '')) {
    const c = zeichen.codePointAt(0);
    if (c < 0x80) n += 1;
    else if (c < 0x800) n += 2;
    else if (c < 0x10000) n += 3;
    else n += 4;
  }
  return n;
}

function istNamensZeichen(c) {
  return (c >= 'a' && c <= 'z') || c === '_';
}

function istBlockName(name) {
  return name === BLOCK_MESSWERTE || name === BLOCK_AUFFAELLIGE;
}

/**
 * Steht auf dieser Zeile nur ein Blockplatzhalter?
 * Spiegel von MailVorlage::istBlockZeile - Leerraum davor und dahinter ist
 * erlaubt, alles andere macht ihn zu einem gewöhnlichen (unbekannten) Platzhalter.
 */
function blockZeile(zeile) {
  const treffer = /^[ \t]*\{([a-z_]+)\}[ \t\r]*$/.exec(zeile);
  if (!treffer || !istBlockName(treffer[1])) return null;
  return treffer[1];
}

/**
 * Wo darf eine zu lange Zeile getrennt werden?
 * Spiegel von MailVorlage::trennstelle - bevorzugt am letzten Leerzeichen,
 * sonst an der letzten UTF-8-Zeichengrenze.
 */
function trenne(bytes, max) {
  if (bytes.length <= max) return bytes.length;
  const mindestens = Math.floor(max / 2);
  for (let i = max; i > mindestens; i--) {
    if (bytes[i - 1] === 0x20) return i;
  }
  let schnitt = max;
  while (schnitt > 0 && (bytes[schnitt] & 0xc0) === 0x80) schnitt--;
  return schnitt > 0 ? schnitt : max;
}

/** Text in UTF-8-Bytes, ohne TextEncoder. */
function zuBytes(text) {
  const raus = [];
  for (const zeichen of String(text)) {
    const c = zeichen.codePointAt(0);
    if (c < 0x80) raus.push(c);
    else if (c < 0x800) raus.push(0xc0 | (c >> 6), 0x80 | (c & 63));
    else if (c < 0x10000) raus.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
    else raus.push(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
  }
  return raus;
}

function vonBytes(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length;) {
    const b = bytes[i];
    if (b < 0x80) { s += String.fromCodePoint(b); i += 1; }
    else if (b < 0xe0) { s += String.fromCodePoint(((b & 31) << 6) | (bytes[i + 1] & 63)); i += 2; }
    else if (b < 0xf0) {
      s += String.fromCodePoint(((b & 15) << 12) | ((bytes[i + 1] & 63) << 6) | (bytes[i + 2] & 63));
      i += 3;
    } else {
      s += String.fromCodePoint(((b & 7) << 18) | ((bytes[i + 1] & 63) << 12) |
                                ((bytes[i + 2] & 63) << 6) | (bytes[i + 3] & 63));
      i += 4;
    }
  }
  return s;
}

/**
 * Trennstelle nach vorn schieben, bis sie außerhalb von Tags und Entitäten
 * liegt - Spiegel von ausserhalbMarkup(). Arbeitet auf Bytes, weil der Umbruch
 * am Gerät auch in Bytes rechnet.
 */
function ausserhalbMarkup(bytes, schnitt) {
  let offen = 0;
  let imTag = false;
  let inEntitaet = false;
  for (let i = 0; i < schnitt; i++) {
    const c = bytes[i];
    if (imTag) {
      if (c === 0x3e) imTag = false; // >
      continue;
    }
    if (inEntitaet) {
      if (c === 0x3b || i - offen > 8) inEntitaet = false; // ;
      continue;
    }
    if (c === 0x3c) { // <
      imTag = true;
      offen = i;
    } else if (c === 0x26) { // &
      inEntitaet = true;
      offen = i;
    }
  }
  if ((imTag || inEntitaet) && offen > 0) return offen;
  return schnitt;
}

/** HTML-Sonderzeichen maskieren - Spiegel von schiebeMaskiert(). */
function maskiere(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

/**
 * Klartext mit Platzhaltern - maskiert, weil der Vorlagentext kein HTML ist.
 * Unbekannte Platzhalter bleiben wörtlich stehen, damit man den Tippfehler
 * sieht statt einer Lücke.
 */
function schiebeText(text, werte) {
  let raus = '';
  let i = 0;
  while (i < text.length) {
    if (text[i] === '{' && text[i + 1] === '{') {
      raus += '{';
      i += 2;
      continue;
    }
    if (text[i] === '{') {
      let ende = i + 1;
      while (ende < text.length && istNamensZeichen(text[ende])) ende++;
      const name = text.slice(i + 1, ende);
      if (name.length > 0 && text[ende] === '}' && werte && name in werte) {
        raus += maskiere(werte[name]);
        i = ende + 1;
        continue;
      }
    }
    raus += maskiere(text[i]);
    i++;
  }
  return raus;
}

/** Nur diese Anfänge werden zu einem Link - Spiegel von istErlaubteAdresse(). */
function istErlaubteAdresse(adresse) {
  return /^(https?:\/\/|mailto:)./.test(adresse);
}

/** **fett** und [Text](Adresse) - Spiegel von schiebeAuszeichnung(). */
function schiebeAuszeichnung(zeile, werte) {
  let raus = '';
  let text = 0;
  let i = 0;
  while (i < zeile.length) {
    if (zeile[i] === '*' && zeile[i + 1] === '*') {
      const zu = zeile.indexOf('**', i + 2);
      if (zu >= 0) {
        raus += schiebeText(zeile.slice(text, i), werte);
        raus += '<strong>' + schiebeText(zeile.slice(i + 2, zu), werte) + '</strong>';
        i = zu + 2;
        text = i;
        continue;
      }
    }
    if (zeile[i] === '[') {
      const txtEnde = zeile.indexOf(']', i + 1);
      if (txtEnde >= 0 && zeile[txtEnde + 1] === '(') {
        const adrEnde = zeile.indexOf(')', txtEnde + 2);
        const adresse = adrEnde >= 0 ? zeile.slice(txtEnde + 2, adrEnde) : '';
        if (adrEnde >= 0 && istErlaubteAdresse(adresse)) {
          raus += schiebeText(zeile.slice(text, i), werte);
          raus += '<a href="' + schiebeText(adresse, werte) + '">' +
                  schiebeText(zeile.slice(i + 1, txtEnde), werte) + '</a>';
          i = adrEnde + 1;
          text = i;
          continue;
        }
      }
    }
    i++;
  }
  return raus + schiebeText(zeile.slice(text), werte);
}

/** Besteht die Zeile nur aus Leerraum? */
function istLeerzeile(zeile) {
  return /^[ \t]*$/.test(zeile);
}

/**
 * Eine Vorlagenzeile in HTML umsetzen.
 * @returns {string[]} keine, eine oder mehrere Ausgabezeilen
 */
function expandiereZeile(zeile, werte, bloecke) {
  const block = blockZeile(zeile);
  if (block) {
    if (!bloecke || !bloecke[block]) return ['<p>' + schiebeText(zeile, werte) + '</p>'];
    return bloecke[block].slice();
  }

  // Leerzeilen gliedern den Editor; die Abstände macht das Design.
  if (istLeerzeile(zeile)) return [];

  const ueberschrift = zeile.startsWith('# ');
  const inhalt = ueberschrift ? zeile.slice(2) : zeile;
  const raus = ueberschrift
    ? '<h1>' + schiebeAuszeichnung(inhalt, werte) + '</h1>'
    : '<p>' + schiebeAuszeichnung(inhalt, werte) + '</p>';

  // Umbruch wie am Gerät, in Bytes gerechnet
  const bytes = zuBytes(raus);
  if (bytes.length <= ZEILE_MAX) return [raus];
  const zeilen = [];
  let at = 0;
  while (at < bytes.length) {
    const rest = bytes.slice(at);
    let nimm = trenne(rest, ZEILE_MAX);
    const sicher = ausserhalbMarkup(rest, nimm);
    if (sicher > 0) nimm = sicher;
    zeilen.push(vonBytes(rest.slice(0, nimm)));
    at += nimm;
  }
  return zeilen;
}

/** Den ganzen Vorlagentext expandieren. */
function expandiere(text, werte, bloecke) {
  return String(text || '')
    .split('\n')
    .reduce((alle, zeile) => alle.concat(expandiereZeile(zeile.replace(/\r$/, ''), werte, bloecke)), [])
    .join('\n');
}

/** Sieht diese Zeile aus wie eine Abschnittsmarke der Vorlagendatei? */
function istMarke(zeile) {
  return /^\[[a-z.]+\]$/.test(zeile);
}

/** Führendes Entwertungszeichen entfernen - Spiegel von MailVorlage::entwerte. */
function entwerte(zeile) {
  return zeile.startsWith('\\') ? zeile.slice(1) : zeile;
}

/**
 * Vorlage prüfen.
 * @returns {{befunde: string[], bytes: number, grenze: number}}
 */
function pruefe(text, grenze) {
  const max = grenze || RUMPF_MAX;
  const befunde = [];
  const zeilen = String(text || '').split('\n');

  zeilen.forEach((zeile, index) => {
    const nummer = index + 1;

    // Unbekannte Platzhalter
    const gefunden = zeile.match(/\{[a-z_]+\}/g) || [];
    gefunden.forEach(roh => {
      const name = roh.slice(1, -1);
      if (!BEKANNT.includes(name)) {
        befunde.push(`Zeile ${nummer}: {${name}} kennt das Gerät nicht - es bleibt so in der Mail stehen.`);
      }
    });

    // Block mitten in der Zeile
    gefunden.forEach(roh => {
      const name = roh.slice(1, -1);
      if (istBlockName(name) && !blockZeile(zeile)) {
        befunde.push(`Zeile ${nummer}: {${name}} setzt eine ganze Tabelle ein und muss allein in einer Zeile stehen.`);
      }
    });

    const zeileBytes = byteLaenge(zeile);
    if (zeileBytes > ZEILE_MAX) {
      befunde.push(`Zeile ${nummer} ist zu lang (${zeileBytes} von ${ZEILE_MAX} Bytes) - mach zwei Absätze daraus.`);
    }

    if (istMarke(zeile)) {
      befunde.push(`Zeile ${nummer}: sieht aus wie eine Abschnittsmarke der Vorlagendatei - das Gerät entwertet sie beim Speichern.`);
    }
  });

  const bytes = byteLaenge(text);
  if (bytes > max) {
    befunde.push(`Zu lang: ${bytes} von ${max} Bytes. Emojis zählen vier Bytes.`);
  }
  return { befunde, bytes, grenze: max };
}

/**
 * Vorschau mit Beispielwerten: dieselbe Mail, die das Gerät zusammensetzt -
 * Rahmen, Stilblock, dann der Text. Spiegel des SendBody-Zweigs in
 * mail_sender.cpp.
 */
function vorschau(text, stil) {
  return '<!DOCTYPE html><html><head><meta charset="utf-8">' +
         '<style>' + String(stil || '').replace(/[<>]/g, '') + '</style></head><body>' +
         expandiere(text, BEISPIEL, BEISPIEL_BLOCK) +
         '</body></html>';
}

// === Verdrahtung (nicht rein) ===

function wire() {
  const rumpf = document.getElementById('vorlage_rumpf');
  const betreff = document.getElementById('vorlage_betreff');
  const zaehler = document.getElementById('vorlage_zaehler');
  const befunde = document.getElementById('vorlage_befunde');
  const rahmen = document.getElementById('vorlage_vorschau');
  const stilFeld = document.getElementById('vorlage_stil');
  if (!rumpf) return;

  // Auf der Designseite steht das CSS im Hauptfeld, sonst im versteckten.
  const istStilSeite = rumpf.name === 'css';
  const grenze = istStilSeite ? STIL_MAX : RUMPF_MAX;

  // Text für die Vorschau, wenn gerade das Design bearbeitet wird: sonst wäre
  // dort nur CSS zu sehen und man könnte die Wirkung nicht beurteilen.
  const BEISPIELTEXT = [
    '# 🌱 So sieht deine Mail aus',
    '',
    'Moin! **{geraet}** meldet sich.',
    '',
    '{messwerte}',
    '',
    '🔗 [Jetzt nachschauen](http://{ip})',
    '🗓️ {datum} um {uhrzeit}'
  ].join('\n');

  /** Zuletzt angeklicktes Textfeld - dorthin gehen Emoji und Platzhalter. */
  let ziel = rumpf;
  [rumpf, betreff].forEach(feld => {
    if (feld) feld.addEventListener('focus', () => { ziel = feld; });
  });

  function stilText() {
    if (istStilSeite) return rumpf.value;
    return stilFeld ? stilFeld.value : '';
  }

  function aktualisiere() {
    const ergebnis = istStilSeite
      ? { befunde: [], bytes: byteLaenge(rumpf.value), grenze }
      : pruefe(rumpf.value, grenze);
    if (istStilSeite && /[<>]/.test(rumpf.value)) {
      ergebnis.befunde.push('Spitze Klammern gehören nicht ins Design - damit ließe sich der Stilblock der Mail verlassen. Das Gerät lehnt das Speichern ab.');
    }
    if (ergebnis.bytes > grenze) {
      ergebnis.befunde.push(`Zu lang: ${ergebnis.bytes} von ${grenze} Bytes.`);
    }

    if (zaehler) {
      zaehler.textContent = `${ergebnis.bytes} / ${grenze} Bytes`;
      zaehler.style.color = ergebnis.bytes > grenze ? '#ff6f6f'
        : (ergebnis.bytes > grenze * 0.9 ? '#ffb020' : '');
    }
    if (befunde) {
      const alle = ergebnis.befunde.slice();
      if (betreff && byteLaenge(betreff.value) > BETREFF_MAX) {
        alle.push(`Betreff zu lang: ${byteLaenge(betreff.value)} von ${BETREFF_MAX} Bytes.`);
      }
      befunde.innerHTML = '';
      alle.forEach(text => {
        const zeile = document.createElement('div');
        zeile.textContent = '⚠️ ' + text;
        befunde.appendChild(zeile);
      });
    }
    if (rahmen) {
      // srcdoc in einem abgeschotteten Rahmen statt innerHTML: sonst zerlegt
      // die Mailgestaltung das Adminlayout, und die eigene Vorlage wäre ein
      // Weg, fremdes Skript in die Seite zu bekommen.
      rahmen.setAttribute('srcdoc',
        vorschau(istStilSeite ? BEISPIELTEXT : rumpf.value, stilText()));
    }
  }

  /** An der Schreibmarke einfügen, statt hinten anzuhängen. */
  function fuegeEin(text) {
    const feld = ziel || rumpf;
    const start = feld.selectionStart || 0;
    const ende = feld.selectionEnd || 0;
    feld.value = feld.value.slice(0, start) + text + feld.value.slice(ende);
    feld.focus();
    feld.selectionStart = feld.selectionEnd = start + text.length;
    aktualisiere();
  }

  rumpf.addEventListener('input', aktualisiere);
  if (betreff) betreff.addEventListener('input', aktualisiere);

  document.querySelectorAll('[data-ph]').forEach(knopf => {
    knopf.addEventListener('click', () => fuegeEin(knopf.dataset.ph));
  });
  document.querySelectorAll('[data-emoji]').forEach(knopf => {
    knopf.addEventListener('click', () => fuegeEin(knopf.dataset.emoji));
  });

  aktualisiere();
}

window.addEventListener('DOMContentLoaded', wire);

// Für die Tests herausgereicht (test/js/mailvorlagen.test.mjs).
if (typeof window !== 'undefined') {
  window.MailVorlagen = {
    byteLaenge, zuBytes, vonBytes, trenne, blockZeile, expandiereZeile, expandiere,
    istMarke, entwerte, pruefe, vorschau, wire, maskiere, schiebeAuszeichnung,
    istLeerzeile, ausserhalbMarkup, istErlaubteAdresse,
    ZEILE_MAX, BETREFF_MAX, RUMPF_MAX, STIL_MAX, BEKANNT, BEISPIEL, BEISPIEL_BLOCK
  };
}
