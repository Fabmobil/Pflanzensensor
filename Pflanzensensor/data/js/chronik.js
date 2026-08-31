/**
 * @file chronik.js
 * @brief Messwertverlauf als Canvas-Diagramm
 *
 * Vom Gerät kommen nur die Rohbytes der Chronik (/chronik/daten); Parsen,
 * Skalieren, Ausdünnen und Zeichnen passiert hier. Das Rahmenformat ist in
 * Pflanzensensor/src/utils/chronik_format.h beschrieben und durch eine
 * gemeinsame Bytefolge in test/js/chronik.test.mjs und
 * test/test_chronik_format/ auf beiden Seiten festgenagelt.
 *
 * Bewusst ohne Bibliothek: das Gerät hat kein Internet, eine CDN-Einbindung
 * wäre offline tot.
 *
 * Beim Entwickeln hart neu laden (Strg+Umschalt+R): statische Dateien gehen
 * mit max-age=86400 raus.
 */

const MAGIC_SAMPLE = 0xc5a5;
const MAGIC_TABLE = 0xc5a7;
const RAW_MAX = 1023;
const MIN_SPAN_S = 300; // 5 Minuten - darunter wird nicht weiter gezoomt
const REFRESH_MS = 60000;
const STATUS_NAMES = ['green', 'yellow', 'red', 'error', 'unknown', 'warmup'];
const COLORS = ['#5ac85a', '#4aa3ff', '#ffb020', '#ff6f91', '#9b7bff', '#22c7c7'];
const RAW_COLOR = '#c9c9c9';

// === Reine Funktionen: Format ===

/** CRC-8 (Polynom 0x07), Spiegel von ChronikFormat::crc8. */
function crc8(bytes, from, to) {
  let crc = 0;
  for (let i = from; i < to; i++) {
    crc ^= bytes[i];
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) & 0xff : (crc << 1) & 0xff;
    }
  }
  return crc;
}

/** IEEE-754 half (16 Bit) nach Number, Spiegel von ChronikFormat::floatFromHalf. */
function halfToFloat(half) {
  const sign = (half & 0x8000) ? -1 : 1;
  const exponent = (half >> 10) & 0x1f;
  const mantissa = half & 0x3ff;
  if (exponent === 0) {
    return sign * mantissa * Math.pow(2, -24); // subnormal, deckt auch ±0 ab
  }
  if (exponent === 0x1f) {
    return mantissa ? NaN : sign * Infinity;
  }
  return sign * (1 + mantissa / 1024) * Math.pow(2, exponent - 15);
}

/**
 * Einen Rahmen ab Position off lesen.
 * @returns {null|object} null, wenn dort kein vollständiger, prüfsummenrichtiger
 *          Rahmen steht - der Aufrufer setzt dann auf dem nächsten Magic auf.
 */
function readFrame(bytes, off) {
  if (off + 8 > bytes.length) return null;
  const magic = bytes[off] | (bytes[off + 1] << 8);
  if (magic !== MAGIC_SAMPLE && magic !== MAGIC_TABLE) return null;

  const epoch = (bytes[off + 2] | (bytes[off + 3] << 8) | (bytes[off + 4] << 16) |
                 (bytes[off + 5] << 24)) >>> 0;
  const count = bytes[off + 6];
  if (count > 16) return null;

  let at = off + 7;
  const eintraege = [];

  if (magic === MAGIC_SAMPLE) {
    for (let i = 0; i < count; i++) {
      if (at + 3 > bytes.length) return null;
      const head = bytes[at++];
      const hasRaw = (head & 0x80) !== 0;
      const wert = halfToFloat(bytes[at] | (bytes[at + 1] << 8));
      at += 2;
      let roh = null;
      if (hasRaw) {
        if (at + 2 > bytes.length) return null;
        const u = bytes[at] | (bytes[at + 1] << 8);
        roh = u > 0x7fff ? u - 0x10000 : u; // int16
        at += 2;
      }
      eintraege.push({
        kanal: head & 0x0f,
        status: STATUS_NAMES[(head >> 4) & 0x07] || 'unknown',
        wert,
        roh
      });
    }
  } else {
    for (let i = 0; i < count; i++) {
      if (at + 2 > bytes.length) return null;
      const kanal = bytes[at++];
      const analog = (bytes[at++] & 0x01) !== 0;
      const texte = [];
      for (let t = 0; t < 3; t++) {
        if (at >= bytes.length) return null;
        const len = bytes[at++];
        if (at + len > bytes.length) return null;
        let s = '';
        for (let k = 0; k < len; k++) s += String.fromCharCode(bytes[at + k]);
        texte.push(decodeUtf8(s));
        at += len;
      }
      if (at + 8 > bytes.length) return null;
      const limits = [];
      for (let l = 0; l < 4; l++) {
        limits.push(halfToFloat(bytes[at] | (bytes[at + 1] << 8)));
        at += 2;
      }
      eintraege.push({
        kanal, analog, schluessel: texte[0], name: texte[1], einheit: texte[2],
        limits: { yellowLow: limits[0], greenLow: limits[1], greenHigh: limits[2], yellowHigh: limits[3] }
      });
    }
  }

  if (at >= bytes.length || bytes[at] !== crc8(bytes, off, at)) return null;
  return { typ: magic === MAGIC_SAMPLE ? 'sample' : 'table', epoch, eintraege, laenge: at + 1 - off };
}

/** Die Texte kommen als UTF-8-Bytes; ohne das stünde "°C" als "Â°C" da. */
function decodeUtf8(raw) {
  try {
    return decodeURIComponent(escape(raw));
  } catch (e) {
    return raw;
  }
}

/** Nächstes Magic ab Position from. */
function findMagic(bytes, from) {
  for (let i = from; i + 1 < bytes.length; i++) {
    const magic = bytes[i] | (bytes[i + 1] << 8);
    if (magic === MAGIC_SAMPLE || magic === MAGIC_TABLE) return i;
  }
  return bytes.length;
}

/**
 * Den ganzen Datenstrom auswerten.
 *
 * Robust gegen Müll und abgeschnittene Rahmen: der Server bricht den Strom ab,
 * wenn ihm der Heap ausgeht, und der letzte Rahmen ist dann unvollständig.
 * Nach einer Störstelle wird auf dem nächsten Magic wieder aufgesetzt.
 */
function parseStream(buffer, vorhanden) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  const tabelle = (vorhanden && vorhanden.tabelle) || new Map();
  const reihen = (vorhanden && vorhanden.reihen) || new Map();
  let verworfen = 0;
  let at = 0;

  while (at < bytes.length) {
    const rahmen = readFrame(bytes, at);
    if (!rahmen) {
      const naechstes = findMagic(bytes, at + 1);
      if (naechstes >= bytes.length) break;
      verworfen++;
      at = naechstes;
      continue;
    }

    if (rahmen.typ === 'table') {
      rahmen.eintraege.forEach(e => tabelle.set(e.kanal, e));
    } else {
      rahmen.eintraege.forEach(e => {
        let reihe = reihen.get(e.kanal);
        if (!reihe) {
          reihe = { t: [], v: [], roh: [], st: [] };
          reihen.set(e.kanal, reihe);
        }
        // Doppelte vermeiden: beim Nachladen mit ?seit= kann der Grenzrahmen
        // erneut kommen.
        const n = reihe.t.length;
        if (n > 0 && reihe.t[n - 1] >= rahmen.epoch) return;
        reihe.t.push(rahmen.epoch);
        reihe.v.push(e.wert);
        reihe.roh.push(e.roh);
        reihe.st.push(e.status);
      });
    }
    at += rahmen.laenge;
  }

  return { tabelle, reihen, verworfen };
}

// === Reine Funktionen: Skalierung und Interaktion ===

/** Umrechnung zwischen Zeit/Wert und Pixeln für einen Zeichenbereich. */
function scale(view, rect) {
  const spanne = Math.max(1, view.bis - view.von);
  const breite = Math.max(1, rect.rechts - rect.links);
  const hoehe = Math.max(1, rect.unten - rect.oben);
  return {
    xToPixel: t => rect.links + ((t - view.von) / spanne) * breite,
    pixelToX: px => view.von + ((px - rect.links) / breite) * spanne,
    yToPixel: (wert, achse) => {
      const bereich = Math.max(1e-9, achse.max - achse.min);
      return rect.unten - ((wert - achse.min) / bereich) * hoehe;
    },
    pixelToY: (py, achse) => {
      const bereich = achse.max - achse.min;
      return achse.min + ((rect.unten - py) / hoehe) * bereich;
    }
  };
}

/** Ausschnitt in die Datengrenzen zwingen, ohne die Spanne zu verändern. */
function clampView(von, bis, grenzen) {
  let spanne = bis - von;
  const gesamt = Math.max(MIN_SPAN_S, grenzen.bis - grenzen.von);
  if (spanne > gesamt) spanne = gesamt;
  if (spanne < MIN_SPAN_S) spanne = MIN_SPAN_S;
  let neuVon = von;
  if (neuVon < grenzen.von) neuVon = grenzen.von;
  if (neuVon + spanne > grenzen.bis) neuVon = grenzen.bis - spanne;
  if (neuVon < grenzen.von) neuVon = grenzen.von;
  return { von: neuVon, bis: neuVon + spanne };
}

/**
 * Zoomen um einen Ankerzeitpunkt.
 * faktor < 1 vergrößert (hineinzoomen), > 1 verkleinert.
 * Der Anker behält seine Bildschirmposition - sonst rutscht die Stelle, die
 * man gerade betrachtet, unter dem Mauszeiger weg.
 */
function zoom(view, faktor, ankerT, grenzen) {
  const spanne = view.bis - view.von;
  const anteil = (ankerT - view.von) / spanne;
  const neueSpanne = Math.max(MIN_SPAN_S, spanne * faktor);
  return clampView(ankerT - anteil * neueSpanne, ankerT + (1 - anteil) * neueSpanne, grenzen);
}

/** Verschieben um dt Sekunden. */
function pan(view, dt, grenzen) {
  return clampView(view.von + dt, view.bis + dt, grenzen);
}

/**
 * Punkte auf Pixelspalten eindampfen.
 *
 * Je Spalte werden Minimum, Maximum und Mittel gebildet - NICHT jeder n-te
 * Punkt genommen. Sonst verschwindet beim Herauszoomen genau die einzelne
 * Spitze, wegen der man sich den Verlauf ansieht. Min und Max ergeben
 * nebenbei die getönte Fläche, die die Schwankungsbreite zeigt.
 */
function bucketize(t, werte, von, bis, pixelBreite) {
  const spalten = Math.max(1, Math.floor(pixelBreite));
  const spanne = Math.max(1e-9, bis - von);
  const eimer = [];
  let aktuell = null;

  for (let i = 0; i < t.length; i++) {
    const zeit = t[i];
    if (zeit < von || zeit > bis) continue;
    const wert = werte[i];
    if (wert === null || wert === undefined || Number.isNaN(wert)) continue;

    const spalte = Math.min(spalten - 1, Math.floor(((zeit - von) / spanne) * spalten));
    if (!aktuell || aktuell.spalte !== spalte) {
      aktuell = { spalte, t: zeit, min: wert, max: wert, summe: 0, anzahl: 0 };
      eimer.push(aktuell);
    }
    if (wert < aktuell.min) aktuell.min = wert;
    if (wert > aktuell.max) aktuell.max = wert;
    aktuell.summe += wert;
    aktuell.anzahl++;
    aktuell.t = zeit;
  }

  return eimer.map(e => ({ t: e.t, min: e.min, max: e.max, avg: e.summe / e.anzahl, anzahl: e.anzahl }));
}

/** Kennzahlen des sichtbaren Ausschnitts für die Legende. */
function windowMinMax(t, werte, von, bis) {
  let min = null, max = null, minT = null, maxT = null, summe = 0, anzahl = 0;
  for (let i = 0; i < t.length; i++) {
    if (t[i] < von || t[i] > bis) continue;
    const wert = werte[i];
    if (wert === null || wert === undefined || Number.isNaN(wert)) continue;
    if (min === null || wert < min) { min = wert; minT = t[i]; }
    if (max === null || wert > max) { max = wert; maxT = t[i]; }
    summe += wert;
    anzahl++;
  }
  return { min, max, minT, maxT, mittel: anzahl ? summe / anzahl : null, anzahl };
}

/**
 * Zusammenhängende Abschnitte finden.
 * Eine Lücke (Gerät aus, kein NTP) oder ein Zeitsprung rückwärts (Neustart)
 * darf nicht als lange gerade Linie durchs Diagramm gezogen werden.
 */
function gaps(t, maxAbstand) {
  const abschnitte = [];
  if (!t.length) return abschnitte;
  let start = 0;
  for (let i = 1; i < t.length; i++) {
    const dt = t[i] - t[i - 1];
    if (dt < 0 || dt > maxAbstand) {
      abschnitte.push([start, i - 1]);
      start = i;
    }
  }
  abschnitte.push([start, t.length - 1]);
  return abschnitte;
}

/** Achsenteilung in runden Schritten (1, 2, 5 × 10^n). */
function niceTicks(min, max, maxTicks) {
  if (!(max > min)) {
    const mitte = Number.isFinite(min) ? min : 0;
    return { min: mitte - 1, max: mitte + 1, schritt: 1, ticks: [mitte - 1, mitte, mitte + 1] };
  }
  const roh = (max - min) / Math.max(1, maxTicks);
  const groesse = Math.pow(10, Math.floor(Math.log10(roh)));
  const rest = roh / groesse;
  const schritt = groesse * (rest > 5 ? 10 : rest > 2 ? 5 : rest > 1 ? 2 : 1);
  const unten = Math.floor(min / schritt) * schritt;
  const oben = Math.ceil(max / schritt) * schritt;
  const ticks = [];
  for (let v = unten; v <= oben + schritt * 1e-6; v += schritt) {
    ticks.push(Math.abs(v) < schritt * 1e-9 ? 0 : v);
  }
  return { min: unten, max: oben, schritt, ticks };
}

/** Zeitachsenbeschriftung, Einheit richtet sich nach der Spanne. */
function timeTicks(von, bis, pixelBreite) {
  const spanne = Math.max(1, bis - von);
  const moegliche = [60, 300, 900, 1800, 3600, 7200, 21600, 43200, 86400, 172800, 604800];
  const gewuenscht = Math.max(2, Math.floor(pixelBreite / 90));
  let schritt = moegliche[moegliche.length - 1];
  for (const kandidat of moegliche) {
    if (spanne / kandidat <= gewuenscht) { schritt = kandidat; break; }
  }
  const ticks = [];
  const ersteStelle = Math.ceil(von / schritt) * schritt;
  for (let t = ersteStelle; t <= bis; t += schritt) {
    ticks.push({ t, text: formatTime(t, schritt) });
  }
  return ticks;
}

function formatTime(epoch, schritt) {
  const d = new Date(epoch * 1000);
  const zwei = n => String(n).padStart(2, '0');
  if (schritt >= 86400) return `${zwei(d.getDate())}.${zwei(d.getMonth() + 1)}.`;
  return `${zwei(d.getHours())}:${zwei(d.getMinutes())}`;
}

/**
 * Aus Kanälen die zeichenbaren Spuren machen.
 *
 * Jeder Kanal liefert eine Wertespur; Analogkanäle zusätzlich eine Rohwertspur.
 * Die Rohwerte sind für die Kalibrierung wichtig - deshalb sind sie einzeln
 * zuschaltbar und stehen als eigene Einträge in der Legende statt hinter einem
 * Sammelschalter. Anfangs sind sie ausgeblendet, sonst wäre das Diagramm beim
 * ersten Blick doppelt so voll.
 */
function tracks(daten, versteckt) {
  const spuren = [];
  daten.tabelle.forEach((info, kanal) => {
    const reihe = daten.reihen.get(kanal);
    if (!reihe) return;
    spuren.push({
      id: String(kanal),
      kanal,
      roh: false,
      name: info.name || ('Kanal ' + kanal),
      einheit: info.einheit || '',
      farbe: COLORS[kanal % COLORS.length],
      werte: reihe.v,
      zeiten: reihe.t,
      aus: versteckt.has(String(kanal))
    });
    if (info.analog) {
      spuren.push({
        id: kanal + 'r',
        kanal,
        roh: true,
        name: (info.name || ('Kanal ' + kanal)) + ' (roh)',
        einheit: 'roh',
        farbe: COLORS[kanal % COLORS.length],
        werte: reihe.roh,
        zeiten: reihe.t,
        aus: versteckt.has(kanal + 'r')
      });
    }
  });
  return spuren;
}

/**
 * Achsen nach Einheit gruppieren, abwechselnd links und rechts.
 *
 * Bewusst ohne Obergrenze: sind viele Sensoren angeschlossen, kommen eben
 * mehrere Skalen zusammen. Eine Reihe wegzulassen, nur weil ihre Einheit die
 * dritte ist, wäre schlechter als ein etwas breiterer Rand - dann fehlte
 * ausgerechnet der Messwert, den jemand sehen will.
 *
 * Gleiche Einheit heißt gleiche Skala; die Reihenfolge richtet sich nach dem
 * ersten Auftreten, damit sich die Achsen beim Ein- und Ausblenden nicht
 * ständig umsortieren.
 */
function axisGroups(spuren) {
  const gruppen = [];
  spuren.forEach(spur => {
    if (spur.aus) return;
    if (!gruppen.some(g => g.einheit === spur.einheit)) {
      gruppen.push({ einheit: spur.einheit, seite: null, ebene: 0 });
    }
  });
  let links = 0, rechts = 0;
  gruppen.forEach((gruppe, index) => {
    if (index % 2 === 0) {
      gruppe.seite = 'links';
      gruppe.ebene = links++;
    } else {
      gruppe.seite = 'rechts';
      gruppe.ebene = rechts++;
    }
  });
  return gruppen;
}

// === Zustand und Zeichnen (nicht rein) ===

const state = {
  daten: { tabelle: new Map(), reihen: new Map(), verworfen: 0 },
  view: { von: 0, bis: 1 },
  grenzen: { von: 0, bis: 1 },
  versteckt: new Set(),
  geladenAb: 0,
  laden: false,
  /// Rohspuren, die schon einmal aufgetaucht sind - siehe versteckeNeueRohspuren()
  rohGesehen: new Set()
};

/** Breite eines Achsenstreifens am Rand. */
const AXIS_WIDTH = 42;

/** Wertebereich je Achsengruppe aus den sichtbaren Spuren bestimmen. */
function achsenBereiche(spuren, gruppen) {
  const bereiche = {};
  gruppen.forEach(gruppe => {
    let min = null, max = null;
    spuren.forEach(spur => {
      if (spur.aus || spur.einheit !== gruppe.einheit) return;
      const mm = windowMinMax(spur.zeiten, spur.werte, state.view.von, state.view.bis);
      if (mm.min === null) return;
      if (min === null || mm.min < min) min = mm.min;
      if (max === null || mm.max > max) max = mm.max;
    });
    if (min === null) { min = 0; max = 1; }
    const rand = (max - min) * 0.08 || 1;
    const skala = niceTicks(min - rand, max + rand, 5);
    bereiche[gruppe.einheit] = {
      min: skala.min, max: skala.max, ticks: skala.ticks, schritt: skala.schritt
    };
  });
  return bereiche;
}

function zeichne() {
  const canvas = document.getElementById('chronik-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const breitePx = canvas.clientWidth || 640;
  const hoehePx = canvas.clientHeight || 320;
  canvas.width = Math.round(breitePx * dpr);
  canvas.height = Math.round(hoehePx * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, breitePx, hoehePx);

  const spuren = tracks(state.daten, state.versteckt);
  const gruppen = axisGroups(spuren);
  const linkeAchsen = gruppen.filter(g => g.seite === 'links').length;
  const rechteAchsen = gruppen.filter(g => g.seite === 'rechts').length;
  const rect = {
    links: 6 + AXIS_WIDTH * Math.max(1, linkeAchsen),
    rechts: breitePx - 6 - AXIS_WIDTH * rechteAchsen,
    oben: 20, // Platz für die Einheitsbeschriftung über dem Diagramm
    unten: hoehePx - 24
  };
  const bereiche = achsenBereiche(spuren, gruppen);
  const s = scale(state.view, rect);

  ctx.font = '10px system-ui, sans-serif';
  ctx.lineWidth = 1;

  // Gitter und Zeitachse
  ctx.strokeStyle = 'rgba(255,255,255,0.12)';
  ctx.fillStyle = 'rgba(255,255,255,0.65)';
  ctx.textAlign = 'center';
  timeTicks(state.view.von, state.view.bis, rect.rechts - rect.links).forEach(tick => {
    const x = s.xToPixel(tick.t);
    ctx.beginPath();
    ctx.moveTo(x, rect.oben);
    ctx.lineTo(x, rect.unten);
    ctx.stroke();
    ctx.fillText(tick.text, x, rect.unten + 14);
  });

  // Wertachsen. Waagerechte Gitterlinien zieht nur die erste - mehrere
  // übereinandergelegte Raster machen das Bild unlesbar.
  gruppen.forEach((gruppe, index) => {
    const bereich = bereiche[gruppe.einheit];
    if (!bereich) return;
    const x = gruppe.seite === 'links'
      ? rect.links - gruppe.ebene * AXIS_WIDTH
      : rect.rechts + gruppe.ebene * AXIS_WIDTH;
    ctx.textAlign = gruppe.seite === 'links' ? 'right' : 'left';
    bereich.ticks.forEach(wert => {
      const y = s.yToPixel(wert, bereich);
      if (y < rect.oben - 1 || y > rect.unten + 1) return;
      if (index === 0) {
        ctx.strokeStyle = 'rgba(255,255,255,0.12)';
        ctx.beginPath();
        ctx.moveTo(rect.links, y);
        ctx.lineTo(rect.rechts, y);
        ctx.stroke();
      }
      ctx.fillStyle = 'rgba(255,255,255,0.6)';
      ctx.fillText(kurz(wert, bereich.schritt), gruppe.seite === 'links' ? x - 4 : x + 4, y + 3);
    });
    // Einheit über dem Zeichenbereich, nicht hinein: sonst überdeckt sie den
    // obersten Tick, und gerade bei engen Wertebereichen liegen die dicht.
    ctx.fillStyle = 'rgba(255,255,255,0.45)';
    ctx.fillText(gruppe.einheit || '–', gruppe.seite === 'links' ? x - 4 : x + 4, 10);
  });

  // Spuren
  spuren.forEach(spur => {
    if (spur.aus) return;
    zeichneReihe(ctx, s, rect, spur.zeiten, spur.werte, bereiche[spur.einheit], spur.farbe, spur.roh);
  });

  const hinweis = document.getElementById('chronik-hint');
  if (hinweis) {
    const etwasDa = spuren.some(spur => !spur.aus && spur.zeiten.length);
    hinweis.style.display = etwasDa ? 'none' : 'block';
    if (!state.daten.reihen.size) hinweis.textContent = 'Noch keine Messwerte aufgezeichnet.';
    else if (!etwasDa) hinweis.textContent = 'Alle Reihen ausgeblendet.';
  }
  baueLegende(spuren);
}

/**
 * Achsenbeschriftung so genau wie nötig.
 *
 * Die Nachkommastellen richten sich nach der Schrittweite, nicht nach dem
 * Betrag: bei einer Temperaturachse von 22,15 bis 22,45 stünde sonst fünfmal
 * "22.2" untereinander.
 */
function kurz(wert, schritt) {
  // Genau so viele Stellen, dass zwei benachbarte Ticks unterscheidbar sind:
  // Schritt 0,05 braucht zwei, Schritt 20 keine.
  const stellen = schritt > 0 ? Math.max(0, Math.min(3, Math.ceil(-Math.log10(schritt)))) : 1;
  return wert.toFixed(stellen);
}

function zeichneReihe(ctx, s, rect, t, werte, achse, farbe, gestrichelt) {
  if (!achse || !t.length) return;
  const eimer = bucketize(t, werte, state.view.von, state.view.bis, rect.rechts - rect.links);
  if (!eimer.length) return;

  const zeiten = eimer.map(e => e.t);
  const abschnitte = gaps(zeiten, gapThreshold(t));
  const mehrfach = eimer.some(e => e.anzahl > 1 && e.max > e.min);

  // Getönte Fläche zwischen Minimum und Maximum je Pixelspalte. Beim
  // Hineinzoomen bis auf Einzelpunkte fällt sie von selbst weg.
  if (mehrfach) {
    ctx.fillStyle = farbe + '33';
    abschnitte.forEach(([von, bis]) => {
      if (bis <= von) return;
      ctx.beginPath();
      for (let i = von; i <= bis; i++) {
        const x = s.xToPixel(eimer[i].t);
        const y = s.yToPixel(eimer[i].max, achse);
        i === von ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
      }
      for (let i = bis; i >= von; i--) {
        ctx.lineTo(s.xToPixel(eimer[i].t), s.yToPixel(eimer[i].min, achse));
      }
      ctx.closePath();
      ctx.fill();
    });
  }

  ctx.strokeStyle = farbe;
  ctx.lineWidth = gestrichelt ? 1 : 1.6;
  if (ctx.setLineDash) ctx.setLineDash(gestrichelt ? [4, 3] : []);
  abschnitte.forEach(([von, bis]) => {
    ctx.beginPath();
    for (let i = von; i <= bis; i++) {
      const x = s.xToPixel(eimer[i].t);
      const y = s.yToPixel(eimer[i].avg, achse);
      i === von ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    if (bis === von) ctx.arc(s.xToPixel(eimer[von].t), s.yToPixel(eimer[von].avg, achse), 1.5, 0, 6.284);
    ctx.stroke();
  });
  if (ctx.setLineDash) ctx.setLineDash([]);

  // Marker auf Minimum und Maximum des sichtbaren Ausschnitts
  if (!gestrichelt) {
    const mm = windowMinMax(t, werte, state.view.von, state.view.bis);
    [[mm.minT, mm.min], [mm.maxT, mm.max]].forEach(([zeit, wert]) => {
      if (zeit === null || wert === null) return;
      ctx.fillStyle = farbe;
      ctx.beginPath();
      ctx.arc(s.xToPixel(zeit), s.yToPixel(wert, achse), 2.5, 0, 6.284);
      ctx.fill();
    });
  }
}

/**
 * Ab welchem Abstand gilt es als Lücke?
 *
 * Das Dreifache des tatsächlichen Takts, nicht ein fester Wert: das
 * Messintervall ist im Adminbereich einstellbar, und bei zehn Minuten Takt
 * wäre jede zweite Linie unterbrochen. Bestimmt wird der Takt aus dem Median
 * der Abstände - ein Mittelwert wäre von einer langen Ausfallzeit verzogen.
 */
function gapThreshold(t) {
  if (t.length < 3) return 180;
  const deltas = [];
  for (let i = 1; i < t.length; i++) {
    const dt = t[i] - t[i - 1];
    if (dt > 0) deltas.push(dt);
  }
  if (!deltas.length) return 180;
  deltas.sort((a, b) => a - b);
  const median = deltas[Math.floor(deltas.length / 2)];
  return Math.max(120, median * 3);
}

function baueLegende(spuren) {
  const box = document.getElementById('chronik-legend');
  if (!box) return;
  box.innerHTML = '';

  spuren.forEach(spur => {
    const mm = windowMinMax(spur.zeiten, spur.werte, state.view.von, state.view.bis);
    const el = document.createElement('div');
    el.className = 'chronik-legend-item' + (spur.aus ? ' aus' : '') + (spur.roh ? ' roh' : '');
    el.dataset.spur = spur.id;
    el.innerHTML =
      `<span class="chronik-dot" style="background:${spur.farbe}"></span>` +
      `<span class="chronik-legend-name">${spur.name}</span>` +
      `<span class="chronik-legend-werte">${zahl(mm.min)} / ${zahl(mm.mittel)} / ${zahl(mm.max)}` +
      `${spur.einheit && spur.einheit !== 'roh' ? ' ' + spur.einheit : ''}</span>`;
    el.title = 'Min / Mittel / Max im sichtbaren Bereich - klicken blendet die Reihe um';
    el.addEventListener('click', () => {
      if (state.versteckt.has(spur.id)) state.versteckt.delete(spur.id);
      else state.versteckt.add(spur.id);
      zeichne();
    });
    box.appendChild(el);
  });
}

function zahl(v) {
  return (v === null || v === undefined || Number.isNaN(v)) ? '–' : (Math.round(v * 10) / 10).toFixed(1);
}

// === Laden ===

function grenzenNeu() {
  let von = null, bis = null;
  state.daten.reihen.forEach(reihe => {
    if (!reihe.t.length) return;
    if (von === null || reihe.t[0] < von) von = reihe.t[0];
    if (bis === null || reihe.t[reihe.t.length - 1] > bis) bis = reihe.t[reihe.t.length - 1];
  });
  if (von === null) {
    const jetzt = Math.floor(Date.now() / 1000);
    von = jetzt - 3600;
    bis = jetzt;
  }
  state.grenzen = { von, bis: Math.max(bis, von + MIN_SPAN_S) };
}

function letzteEpoche() {
  let letzte = 0;
  state.daten.reihen.forEach(reihe => {
    const n = reihe.t.length;
    if (n && reihe.t[n - 1] > letzte) letzte = reihe.t[n - 1];
  });
  return letzte;
}

/**
 * Daten holen. Beim ersten Aufruf nur das gewünschte Fenster, danach immer
 * nur den Zuwachs - der ESP beantwortet jeweils nur eine Verbindung, ein
 * voller Abzug bei jedem Auffrischen würde ihn dauerhaft beschäftigen.
 */
function lade(seit, anhaengen) {
  if (state.laden) return Promise.resolve();
  state.laden = true;
  const url = '/chronik/daten' + (seit > 0 ? ('?seit=' + seit) : '');
  return fetch(url)
    .then(antwort => {
      if (!antwort.ok) throw new Error('HTTP ' + antwort.status);
      return antwort.arrayBuffer();
    })
    .then(buffer => {
      state.daten = parseStream(buffer, anhaengen ? state.daten : null);
      versteckeNeueRohspuren();
      if (!anhaengen) state.geladenAb = seit;
      grenzenNeu();
      if (!anhaengen || state.view.bis >= letzteEpoche() - 120) {
        setzeBereich(state.view.bis - state.view.von);
      }
      zeichne();
    })
    .catch(fehler => {
      console.error('Chronik konnte nicht geladen werden:', fehler);
      const hinweis = document.getElementById('chronik-hint');
      if (hinweis) {
        hinweis.style.display = 'block';
        hinweis.textContent = 'Daten konnten nicht geladen werden.';
      }
    })
    .then(() => { state.laden = false; });
}

/**
 * Neu hinzugekommene Rohspuren ausblenden.
 *
 * Nur einmal je Spur, damit ein bewusstes Einschalten nicht beim nächsten
 * Auffrischen wieder zurückgesetzt wird.
 */
function versteckeNeueRohspuren() {
  state.daten.tabelle.forEach((info, kanal) => {
    const id = kanal + 'r';
    if (info.analog && !state.rohGesehen.has(id)) {
      state.rohGesehen.add(id);
      state.versteckt.add(id);
    }
  });
}

function setzeBereich(spanne) {
  const bis = state.grenzen.bis;
  const von = spanne > 0 ? bis - spanne : state.grenzen.von;
  state.view = clampView(von, bis, state.grenzen);
}

// === Verdrahtung ===

function wire() {
  const canvas = document.getElementById('chronik-canvas');
  if (!canvas) return;

  canvas.addEventListener('wheel', ereignis => {
    ereignis.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const zeichenbereich = { links: 46, rechts: rect.width - 46, oben: 10, unten: rect.height - 24 };
    const s = scale(state.view, zeichenbereich);
    const anker = s.pixelToX(ereignis.clientX - rect.left);
    state.view = zoom(state.view, ereignis.deltaY > 0 ? 1.25 : 0.8, anker, state.grenzen);
    zeichne();
  }, { passive: false });

  let ziehtAb = null;
  canvas.addEventListener('pointerdown', ereignis => {
    ziehtAb = { x: ereignis.clientX, view: { ...state.view } };
    if (canvas.setPointerCapture) canvas.setPointerCapture(ereignis.pointerId);
  });
  canvas.addEventListener('pointermove', ereignis => {
    if (!ziehtAb) return;
    const rect = canvas.getBoundingClientRect();
    const breite = Math.max(1, rect.width - 92);
    const spanne = ziehtAb.view.bis - ziehtAb.view.von;
    const dt = -((ereignis.clientX - ziehtAb.x) / breite) * spanne;
    state.view = pan(ziehtAb.view, dt, state.grenzen);
    zeichne();
  });
  const losgelassen = () => { ziehtAb = null; };
  canvas.addEventListener('pointerup', losgelassen);
  canvas.addEventListener('pointercancel', losgelassen);

  const ranges = document.getElementById('chronik-ranges');
  if (ranges) {
    ranges.addEventListener('click', ereignis => {
      const knopf = ereignis.target.closest ? ereignis.target.closest('[data-range]') : null;
      if (!knopf) return;
      const spanne = parseInt(knopf.dataset.range, 10);
      ranges.querySelectorAll('[data-range]').forEach(b => {
        b.className = b === knopf ? 'button-primary' : 'button-secondary';
      });
      // Für einen größeren Ausschnitt müssen erst die älteren Daten her.
      const braucht = spanne === 0 ? 0 : Math.max(0, Math.floor(Date.now() / 1000) - spanne);
      if (state.geladenAb === 0 || (braucht !== 0 && braucht >= state.geladenAb)) {
        setzeBereich(spanne);
        zeichne();
      } else {
        lade(braucht, false).then(() => { setzeBereich(spanne); zeichne(); });
      }
    });
  }

  window.addEventListener('resize', zeichne);
}

window.addEventListener('DOMContentLoaded', () => {
  wire();
  const start = Math.max(0, Math.floor(Date.now() / 1000) - 86400);
  lade(start, false);
  setInterval(() => {
    if (document.visibilityState === 'hidden') return;
    const letzte = letzteEpoche();
    lade(letzte > 0 ? letzte : 0, true);
  }, REFRESH_MS);
});

// Für die Tests herausgereicht (test/js/chronik.test.mjs).
if (typeof window !== 'undefined') {
  window.Chronik = {
    crc8, halfToFloat, readFrame, findMagic, parseStream,
    scale, clampView, zoom, pan, bucketize, windowMinMax, gaps,
    niceTicks, timeTicks, tracks, axisGroups, gapThreshold, kurz, zeichne, wire, lade, state,
    versteckeNeueRohspuren,
    MIN_SPAN_S
  };
}
