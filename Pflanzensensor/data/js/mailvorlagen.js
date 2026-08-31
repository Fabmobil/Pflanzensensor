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

const BLOCK_MESSWERTE = 'messwerte';
const BLOCK_AUFFAELLIGE = 'auffaellige';
const BEKANNT = ['geraet', 'ip', 'ssid', 'neustarts', 'laufzeit', 'datum', 'uhrzeit',
                 BLOCK_MESSWERTE, BLOCK_AUFFAELLIGE];

/** Beispielwerte für die Vorschau - dieselbe Auswahl wie am Gerät. */
const BEISPIEL = {
  geraet: 'Frameclaw PS',
  ip: '192.168.1.44',
  ssid: 'Magrathea',
  neustarts: '7',
  laufzeit: '2d 5h 13m',
  datum: '31.08.2026',
  uhrzeit: '18:24'
};

const BEISPIEL_BLOCK = {
  messwerte: [
    '<tr><td style="padding:5px 0">🟢 Bodenfeuchte</td><td style="padding:5px 0;text-align:right"><b>42.0%</b></td></tr>',
    '<tr><td style="padding:5px 0">🟡 Lichtstärke</td><td style="padding:5px 0;text-align:right"><b>12.5%</b></td></tr>',
    '<tr><td style="padding:5px 0">🟢 Lufttemperatur</td><td style="padding:5px 0;text-align:right"><b>22.4°C</b></td></tr>'
  ],
  auffaellige: [
    '<tr><td style="padding:5px 0">🟡 Lichtstärke</td><td style="padding:5px 0;text-align:right"><b>12.5%</b></td></tr>'
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
 * Eine Vorlagenzeile expandieren.
 * @returns {string[]} eine oder mehrere Ausgabezeilen
 */
function expandiereZeile(zeile, werte, bloecke) {
  const block = blockZeile(zeile);
  if (block) {
    if (!bloecke || !bloecke[block]) return [zeile];
    return bloecke[block].slice();
  }

  let raus = '';
  let i = 0;
  while (i < zeile.length) {
    if (zeile[i] === '{' && zeile[i + 1] === '{') {
      raus += '{';
      i += 2;
      continue;
    }
    if (zeile[i] === '{') {
      let ende = i + 1;
      while (ende < zeile.length && istNamensZeichen(zeile[ende])) ende++;
      const name = zeile.slice(i + 1, ende);
      if (name.length > 0 && zeile[ende] === '}' && werte && name in werte) {
        raus += werte[name];
        i = ende + 1;
        continue;
      }
      // Unbekannt oder unabgeschlossen: die Klammer bleibt stehen
    }
    raus += zeile[i];
    i++;
  }

  // Umbruch wie am Gerät, in Bytes gerechnet
  const bytes = zuBytes(raus);
  if (bytes.length <= ZEILE_MAX) return [raus];
  const zeilen = [];
  let at = 0;
  while (at < bytes.length) {
    const rest = bytes.slice(at);
    const nimm = trenne(rest, ZEILE_MAX);
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
        befunde.push(`Zeile ${nummer}: {${name}} setzt ganze Tabellenzeilen ein und muss allein in einer Zeile stehen.`);
      }
    });

    const zeileBytes = byteLaenge(zeile);
    if (zeileBytes > ZEILE_MAX) {
      befunde.push(`Zeile ${nummer} ist zu lang (${zeileBytes} von ${ZEILE_MAX} Bytes) - bitte aufteilen. Im HTML ist ein Umbruch zwischen Tags bedeutungslos.`);
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

/** Vorschau mit Beispielwerten. */
function vorschau(text) {
  return expandiere(text, BEISPIEL, BEISPIEL_BLOCK);
}

// === Verdrahtung (nicht rein) ===

function wire() {
  const rumpf = document.getElementById('vorlage_rumpf');
  const betreff = document.getElementById('vorlage_betreff');
  const zaehler = document.getElementById('vorlage_zaehler');
  const befunde = document.getElementById('vorlage_befunde');
  const rahmen = document.getElementById('vorlage_vorschau');
  if (!rumpf) return;

  function aktualisiere() {
    const ergebnis = pruefe(rumpf.value, RUMPF_MAX);
    if (zaehler) {
      zaehler.textContent = `${ergebnis.bytes} / ${ergebnis.grenze} Bytes`;
      zaehler.style.color = ergebnis.bytes > ergebnis.grenze ? '#ff6f6f'
        : (ergebnis.bytes > ergebnis.grenze * 0.9 ? '#ffb020' : '');
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
      rahmen.setAttribute('srcdoc', vorschau(rumpf.value));
    }
  }

  rumpf.addEventListener('input', aktualisiere);
  if (betreff) betreff.addEventListener('input', aktualisiere);

  const liste = document.getElementById('vorlage_platzhalter');
  document.querySelectorAll('[data-ph]').forEach(knopf => {
    knopf.addEventListener('click', () => {
      const text = knopf.dataset.ph;
      const start = rumpf.selectionStart || 0;
      const ende = rumpf.selectionEnd || 0;
      rumpf.value = rumpf.value.slice(0, start) + text + rumpf.value.slice(ende);
      rumpf.focus();
      rumpf.selectionStart = rumpf.selectionEnd = start + text.length;
      aktualisiere();
    });
  });
  void liste;

  aktualisiere();
}

window.addEventListener('DOMContentLoaded', wire);

// Für die Tests herausgereicht (test/js/mailvorlagen.test.mjs).
if (typeof window !== 'undefined') {
  window.MailVorlagen = {
    byteLaenge, zuBytes, vonBytes, trenne, blockZeile, expandiereZeile, expandiere,
    istMarke, entwerte, pruefe, vorschau, wire,
    ZEILE_MAX, BETREFF_MAX, RUMPF_MAX, BEKANNT, BEISPIEL, BEISPIEL_BLOCK
  };
}
