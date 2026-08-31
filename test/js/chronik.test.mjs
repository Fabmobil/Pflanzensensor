/**
 * Tests für data/js/chronik.js - das Diagramm der Chronik.
 *
 * Zwei Dinge stehen hier im Vordergrund. Erstens die Formatklammer: die
 * Bytefolge in REFERENZ_RAHMEN ist dieselbe wie in
 * test/test_chronik_format/test_chronik_format.cpp. Ändert eine Seite das
 * Rahmenformat, fällt es sofort auf statt erst am Gerät als leeres Diagramm.
 * Zweitens die Rechenteile hinter Zoom, Ziehen und Ausdünnen - die sieht man
 * beim Ausprobieren nur, wenn man genau die kaputte Stelle erwischt.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadChronik, makeClock } from './helpers/load.mjs';

const C = loadChronik();

/**
 * Werte aus der Sandbox in Node-eigene Strukturen überführen.
 *
 * chronik.js läuft in einem eigenen vm-Realm; ein dort erzeugtes Array hat
 * einen anderen Prototyp als eines von Node. assert.deepEqual meldet dann
 * "same structure but not reference-equal", obwohl inhaltlich alles stimmt.
 */
const alsWerte = (x) => JSON.parse(JSON.stringify(x));

/** Derselbe Messrahmen wie im nativen Test: epoch 1756612345, zwei Kanäle. */
const REFERENZ_RAHMEN = Uint8Array.from([
  0xa5, 0xc5, 0xf9, 0xc6, 0xb3, 0x68, 0x02, 0x80,
  0x26, 0x50, 0x92, 0x02, 0x13, 0x40, 0xca, 0xe0
]);

/** Kanaltabelle bauen, wie der TableBuilder sie schreibt. */
function tabellenRahmen(epoch, kanaele) {
  const bytes = [0xa7, 0xc5];
  bytes.push(epoch & 0xff, (epoch >> 8) & 0xff, (epoch >> 16) & 0xff, (epoch >>> 24) & 0xff);
  bytes.push(kanaele.length);
  for (const k of kanaele) {
    bytes.push(k.kanal, k.analog ? 1 : 0);
    for (const text of [k.schluessel, k.name, k.einheit]) {
      const roh = Buffer.from(text, 'utf8');
      bytes.push(roh.length, ...roh);
    }
    for (const grenze of [10, 20, 80, 90]) {
      const h = floatZuHalb(grenze);
      bytes.push(h & 0xff, (h >> 8) & 0xff);
    }
  }
  bytes.push(C.crc8(bytes, 0, bytes.length));
  return Uint8Array.from(bytes);
}

function messRahmen(epoch, werte) {
  const bytes = [0xa5, 0xc5];
  bytes.push(epoch & 0xff, (epoch >> 8) & 0xff, (epoch >> 16) & 0xff, (epoch >>> 24) & 0xff);
  bytes.push(werte.length);
  for (const w of werte) {
    const status = ['green', 'yellow', 'red', 'error', 'unknown', 'warmup'].indexOf(w.status || 'green');
    bytes.push((w.kanal & 0x0f) | (status << 4) | (w.roh === undefined ? 0 : 0x80));
    const h = floatZuHalb(w.wert);
    bytes.push(h & 0xff, (h >> 8) & 0xff);
    if (w.roh !== undefined) bytes.push(w.roh & 0xff, (w.roh >> 8) & 0xff);
  }
  bytes.push(C.crc8(bytes, 0, bytes.length));
  return Uint8Array.from(bytes);
}

/** Unabhängig von chronik.js: Node kann half über einen typisierten Umweg. */
function floatZuHalb(v) {
  // Node kennt kein Float16Array; die Umrechnung steht deshalb hier von Hand
  // und ist bewusst NICHT aus chronik.js entliehen - sonst prüfte der Test
  // seine eigene Annahme.
  const buf = new DataView(new ArrayBuffer(2));
  const f32 = new DataView(new ArrayBuffer(4));
  f32.setFloat32(0, v);
  const bits = f32.getUint32(0);
  const sign = (bits >>> 16) & 0x8000;
  let exponent = ((bits >>> 23) & 0xff) - 127;
  const mantissa = bits & 0x7fffff;
  if (exponent > 15) return sign | 0x7c00;
  if (exponent < -14) return sign;
  buf.setUint16(0, sign | ((exponent + 15) << 10) | (mantissa >> 13));
  return buf.getUint16(0);
}

function verkette(...teile) {
  const gesamt = teile.reduce((n, t) => n + t.length, 0);
  const raus = new Uint8Array(gesamt);
  let at = 0;
  for (const t of teile) { raus.set(t, at); at += t.length; }
  return raus;
}

// === Format ===

test('halfToFloat trifft dieselben Werte wie die C++-Seite', () => {
  assert.equal(C.halfToFloat(0x0000), 0);
  assert.equal(C.halfToFloat(0x3c00), 1);
  assert.equal(C.halfToFloat(0xbc00), -1);
  assert.equal(C.halfToFloat(0x3800), 0.5);
  assert.equal(C.halfToFloat(0x5640), 100);
  assert.equal(C.halfToFloat(0x6ce2), 5000);
  assert.equal(C.halfToFloat(0x7bff), 65504);
  assert.equal(C.halfToFloat(0x4e58), 25.375);
  assert.equal(C.halfToFloat(0xca40), -12.5);
  assert.ok(Number.isNaN(C.halfToFloat(0x7e00)));
});

test('crc8 liefert den Standardprüfwert', () => {
  const daten = Uint8Array.from(Buffer.from('123456789', 'ascii'));
  assert.equal(C.crc8(daten, 0, daten.length), 0xf4);
});

test('readFrame liest den Rahmen aus dem nativen Test', () => {
  // Diese Zusicherung ist die Klammer zwischen C++ und JavaScript.
  const rahmen = C.readFrame(REFERENZ_RAHMEN, 0);
  assert.ok(rahmen, 'Rahmen muss gelesen werden');
  assert.equal(rahmen.typ, 'sample');
  assert.equal(rahmen.epoch, 1756612345);
  assert.equal(rahmen.laenge, 16);
  assert.equal(rahmen.eintraege.length, 2);

  assert.equal(rahmen.eintraege[0].kanal, 0);
  assert.equal(rahmen.eintraege[0].status, 'green');
  assert.ok(Math.abs(rahmen.eintraege[0].wert - 33.2) < 0.02);
  assert.equal(rahmen.eintraege[0].roh, 658);

  assert.equal(rahmen.eintraege[1].kanal, 3);
  assert.equal(rahmen.eintraege[1].status, 'yellow');
  assert.equal(rahmen.eintraege[1].wert, -12.5);
  assert.equal(rahmen.eintraege[1].roh, null);
});

test('readFrame lehnt beschädigte und angeschnittene Rahmen ab', () => {
  const kaputt = Uint8Array.from(REFERENZ_RAHMEN);
  kaputt[8] ^= 0x01;
  assert.equal(C.readFrame(kaputt, 0), null);

  for (let len = 0; len < REFERENZ_RAHMEN.length; len++) {
    assert.equal(C.readFrame(REFERENZ_RAHMEN.slice(0, len), 0), null, `Präfix der Länge ${len}`);
  }
});

test('parseStream baut Tabelle und Reihen auf', () => {
  const strom = verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Lichtstärke', einheit: '%' },
      { kanal: 2, analog: false, schluessel: 'DHT_0', name: 'Lufttemperatur', einheit: '°C' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 33.5, roh: 658 }, { kanal: 2, wert: 23.5 }]),
    messRahmen(1756612120, [{ kanal: 0, wert: 34.0, roh: 662 }, { kanal: 2, wert: 23.75 }])
  );

  const daten = C.parseStream(strom, null);
  assert.equal(daten.verworfen, 0);
  assert.equal(daten.tabelle.get(0).schluessel, 'ANALOG_0');
  assert.equal(daten.tabelle.get(0).einheit, '%');
  // Umlaute und Gradzeichen kommen als UTF-8-Bytes und müssen dekodiert werden
  assert.equal(daten.tabelle.get(0).name, 'Lichtstärke');
  assert.equal(daten.tabelle.get(2).einheit, '°C');
  assert.equal(daten.tabelle.get(0).limits.greenHigh, 80);

  const reihe = daten.reihen.get(0);
  assert.deepEqual(alsWerte(reihe.t), [1756612060, 1756612120]);
  assert.equal(reihe.roh[1], 662);
  assert.equal(daten.reihen.get(2).v.length, 2);
});

test('parseStream setzt nach Müll wieder auf', () => {
  const muell = Uint8Array.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]);
  const strom = verkette(
    messRahmen(1756612060, [{ kanal: 0, wert: 10 }]),
    muell,
    messRahmen(1756612120, [{ kanal: 0, wert: 11 }])
  );

  const daten = C.parseStream(strom, null);
  assert.equal(daten.reihen.get(0).t.length, 2);
  assert.ok(daten.verworfen >= 1, 'die Störstelle muss gezählt werden');
});

test('parseStream verträgt einen abgeschnittenen letzten Rahmen', () => {
  // Genau das passiert, wenn der Server den Strom bei knappem Heap abbricht.
  const voll = verkette(
    messRahmen(1756612060, [{ kanal: 0, wert: 10 }]),
    messRahmen(1756612120, [{ kanal: 0, wert: 11 }])
  );
  const daten = C.parseStream(voll.slice(0, voll.length - 3), null);
  assert.equal(daten.reihen.get(0).t.length, 1);
});

test('parseStream hängt an und überspringt doppelte Zeitstempel', () => {
  // Beim Nachladen mit ?seit= kommt der Grenzrahmen ein zweites Mal.
  const erste = C.parseStream(messRahmen(1756612060, [{ kanal: 0, wert: 10 }]), null);
  const zweite = C.parseStream(
    verkette(messRahmen(1756612060, [{ kanal: 0, wert: 10 }]),
             messRahmen(1756612120, [{ kanal: 0, wert: 11 }])),
    erste);
  assert.deepEqual(alsWerte(zweite.reihen.get(0).t), [1756612060, 1756612120]);
});

// === Skalierung, Zoom, Ziehen ===

const RECT = { links: 46, rechts: 646, oben: 10, unten: 300 };

test('scale rechnet verlustfrei hin und zurück', () => {
  const s = C.scale({ von: 1000, bis: 4600 }, RECT);
  for (const t of [1000, 2000, 3333, 4600]) {
    assert.ok(Math.abs(s.pixelToX(s.xToPixel(t)) - t) < 1e-6, `t=${t}`);
  }
  assert.equal(s.xToPixel(1000), RECT.links);
  assert.equal(s.xToPixel(4600), RECT.rechts);

  const achse = { min: 0, max: 100 };
  assert.equal(s.yToPixel(0, achse), RECT.unten);
  assert.equal(s.yToPixel(100, achse), RECT.oben);
});

test('zoom hält den Anker an seiner Stelle', () => {
  const grenzen = { von: 0, bis: 100000 };
  const view = { von: 10000, bis: 20000 };
  const anker = 12000;

  const vorher = C.scale(view, RECT).xToPixel(anker);
  const neu = C.zoom(view, 0.5, anker, grenzen);
  const nachher = C.scale(neu, RECT).xToPixel(anker);

  assert.ok(Math.abs(vorher - nachher) < 1e-6, 'der Anker darf nicht wegrutschen');
  assert.ok(neu.bis - neu.von < view.bis - view.von, 'es muss näher herangehen');
});

test('zoom klemmt an den Datengrenzen und an der Mindestspanne', () => {
  const grenzen = { von: 1000, bis: 5000 };
  const weit = C.zoom({ von: 2000, bis: 3000 }, 100, 2500, grenzen);
  assert.equal(weit.von, 1000);
  assert.equal(weit.bis, 5000);

  const eng = C.zoom({ von: 2000, bis: 3000 }, 0.0001, 2500, grenzen);
  assert.equal(eng.bis - eng.von, C.MIN_SPAN_S);
});

test('pan behält die Spanne und verlässt den Datenbereich nicht', () => {
  const grenzen = { von: 1000, bis: 5000 };
  const view = { von: 2000, bis: 3000 };

  const links = C.pan(view, -10000, grenzen);
  assert.equal(links.von, 1000);
  assert.equal(links.bis - links.von, 1000);

  const rechts = C.pan(view, 10000, grenzen);
  assert.equal(rechts.bis, 5000);
  assert.equal(rechts.bis - rechts.von, 1000);
});

// === Ausdünnen und Kennzahlen ===

test('bucketize dünnt aus, ohne eine Spitze zu verschlucken', () => {
  const t = [], v = [];
  for (let i = 0; i < 10000; i++) { t.push(1000 + i); v.push(50); }
  v[5000] = 99; // die eine Spitze, wegen der man hinsieht

  const eimer = C.bucketize(t, v, 1000, 11000, 800);
  assert.ok(eimer.length <= 800, `zu viele Punkte: ${eimer.length}`);
  assert.ok(eimer.some(e => e.max === 99), 'die Spitze muss im Maximum überleben');
  assert.ok(eimer.every(e => e.min <= e.avg && e.avg <= e.max));
});

test('bucketize verträgt leere Eingaben und leere Bereiche', () => {
  assert.equal(C.bucketize([], [], 0, 100, 800).length, 0);
  assert.equal(C.bucketize([10, 20], [1, 2], 500, 600, 800).length, 0);
});

test('windowMinMax betrachtet nur den sichtbaren Ausschnitt', () => {
  const t = [100, 200, 300, 400];
  const v = [1, 99, 5, -7];
  const mm = C.windowMinMax(t, v, 150, 350);
  assert.equal(mm.min, 5);
  assert.equal(mm.max, 99);
  assert.equal(mm.minT, 300);
  assert.equal(mm.maxT, 200);
  assert.equal(mm.anzahl, 2);
  assert.equal(mm.mittel, 52);

  const leer = C.windowMinMax(t, v, 1000, 2000);
  assert.equal(leer.min, null);
  assert.equal(leer.mittel, null);
});

test('gaps trennt bei Lücken und bei Zeitsprüngen rückwärts', () => {
  // 60-Sekunden-Takt mit einem Loch von zehn Minuten
  const mitLoch = [0, 60, 120, 720, 780];
  assert.deepEqual(alsWerte(C.gaps(mitLoch, 180)), [[0, 2], [3, 4]]);

  // Nach einem Neustart springt die Uhr zurück - keine Linie quer durchs Bild
  const rueckwaerts = [1000, 1060, 500, 560];
  assert.deepEqual(alsWerte(C.gaps(rueckwaerts, 180)), [[0, 1], [2, 3]]);

  assert.equal(C.gaps([], 180).length, 0);
  assert.deepEqual(alsWerte(C.gaps([42], 180)), [[0, 0]]);
});

test('gapThreshold richtet sich nach dem tatsächlichen Messtakt', () => {
  // Minutentakt: ein Loch von zehn Minuten ist eine Lücke, 60 s nicht
  const minutentakt = [0, 60, 120, 180, 240];
  assert.equal(C.gapThreshold(minutentakt), 180);

  // Zehnminutentakt (im Adminbereich einstellbar): die Schwelle wächst mit,
  // sonst wäre jede Linie unterbrochen
  const langsam = [0, 600, 1200, 1800];
  assert.equal(C.gapThreshold(langsam), 1800);

  // Eine einzelne lange Ausfallzeit darf die Schwelle nicht verziehen
  const mitAusfall = [0, 60, 120, 180, 90000, 90060];
  assert.equal(C.gapThreshold(mitAusfall), 180);

  // Zu wenig Punkte für einen Median: Rückfall auf einen festen Wert
  assert.equal(C.gapThreshold([]), 180);
  assert.equal(C.gapThreshold([100, 160]), 180);
});

test('kurz rundet nach der Schrittweite, nicht nach dem Betrag', () => {
  // Enge Temperaturachse: fünfmal "22.2" untereinander wäre nutzlos
  assert.equal(C.kurz(22.15, 0.05), '22.15');
  assert.equal(C.kurz(22.2, 0.05), '22.20');
  assert.notEqual(C.kurz(22.15, 0.05), C.kurz(22.2, 0.05));

  // Grobe Achse braucht keine Nachkommastellen
  assert.equal(C.kurz(1000, 200), '1000');
  assert.equal(C.kurz(80, 20), '80');
});

test('niceTicks liefert runde Schritte und verträgt min == max', () => {
  const t = C.niceTicks(0, 100, 5);
  assert.equal(t.schritt, 20);
  assert.equal(t.min, 0);
  assert.equal(t.max, 100);

  const flach = C.niceTicks(7, 7, 5);
  assert.ok(Number.isFinite(flach.schritt) && flach.schritt > 0);
  assert.ok(flach.max > flach.min);
});

test('timeTicks wählt die Einheit nach der Spanne', () => {
  const kurz = C.timeTicks(1756612000, 1756615600, 600); // eine Stunde
  assert.ok(kurz.length >= 2 && kurz.length <= 12);
  assert.match(kurz[0].text, /^\d\d:\d\d$/);

  const lang = C.timeTicks(1756612000, 1756612000 + 7 * 86400, 600);
  assert.match(lang[0].text, /^\d\d\.\d\d\.$/);
});

// === Achsen und Legende ===

test('tracks legt für Analogkanäle eine eigene Rohspur an', () => {
  const daten = C.parseStream(verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Lichtstärke', einheit: '%' },
      { kanal: 2, analog: false, schluessel: 'DHT_0', name: 'Lufttemperatur', einheit: '°C' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 30, roh: 600 }, { kanal: 2, wert: 20 }])
  ), null);

  const spuren = C.tracks(daten, new Set());
  assert.equal(spuren.length, 3, 'zwei Messwerte plus eine Rohspur');

  const roh = spuren.find(s => s.roh);
  assert.equal(roh.id, '0r');
  assert.equal(roh.einheit, 'roh');
  assert.equal(roh.kanal, 0);
  assert.ok(roh.name.includes('roh'));
  // Der Rohwert kommt aus roh[], nicht aus v[]
  assert.equal(alsWerte(roh.werte)[0], 600);

  // Ein nicht-analoger Kanal bekommt keine Rohspur
  assert.equal(spuren.filter(s => s.kanal === 2).length, 1);
});

test('tracks übernimmt den Ausblendzustand je Spur', () => {
  const daten = C.parseStream(verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Licht', einheit: '%' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 30, roh: 600 }])
  ), null);

  const spuren = C.tracks(daten, new Set(['0r']));
  assert.equal(spuren.find(s => !s.roh).aus, false);
  assert.equal(spuren.find(s => s.roh).aus, true, 'nur die Rohspur ist aus');
});

test('axisGroups verteilt jede Einheit abwechselnd nach links und rechts', () => {
  const spuren = [
    { einheit: '%', aus: false }, { einheit: '°C', aus: false },
    { einheit: 'ppm', aus: false }, { einheit: 'roh', aus: false }
  ];
  const gruppen = alsWerte(C.axisGroups(spuren));

  assert.equal(gruppen.length, 4, 'keine Einheit wird weggelassen');
  assert.deepEqual(gruppen.map(g => g.einheit), ['%', '°C', 'ppm', 'roh']);
  assert.deepEqual(gruppen.map(g => g.seite), ['links', 'rechts', 'links', 'rechts']);
  // Die zweite Achse derselben Seite rückt nach außen
  assert.deepEqual(gruppen.map(g => g.ebene), [0, 0, 1, 1]);
});

test('axisGroups fasst gleiche Einheiten zusammen und ignoriert Ausgeblendete', () => {
  const gruppen = alsWerte(C.axisGroups([
    { einheit: '%', aus: false },
    { einheit: '%', aus: false },
    { einheit: '°C', aus: true }
  ]));
  assert.equal(gruppen.length, 1);
  assert.equal(gruppen[0].einheit, '%');
});

test('Rohspuren sind anfangs ausgeblendet, ein Einschalten überlebt das Auffrischen', () => {
  const K = loadChronik();
  K.state.daten = K.parseStream(verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Licht', einheit: '%' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 30, roh: 600 }])
  ), null);

  K.versteckeNeueRohspuren();
  assert.ok(K.state.versteckt.has('0r'), 'Rohwerte starten ausgeblendet');

  // Nutzer schaltet sie ein
  K.state.versteckt.delete('0r');
  // Auffrischen darf sie nicht wieder ausblenden
  K.versteckeNeueRohspuren();
  assert.ok(!K.state.versteckt.has('0r'));
});

// === Zeichnen und Legende gegen den Fake-Canvas ===

test('zeichne baut die Legende und malt Linien', () => {
  const K = loadChronik();
  const strom = verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Lichtstärke', einheit: '%' },
      { kanal: 2, analog: false, schluessel: 'DHT_0', name: 'Lufttemperatur', einheit: '°C' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 30, roh: 600 }, { kanal: 2, wert: 20 }]),
    messRahmen(1756612120, [{ kanal: 0, wert: 40, roh: 700 }, { kanal: 2, wert: 22 }])
  );
  K.state.daten = K.parseStream(strom, null);
  K.state.grenzen = { von: 1756612060, bis: 1756612120 };
  K.state.view = { von: 1756612060, bis: 1756612120 };

  K.zeichne();

  const aufrufe = K._canvas._ctx._calls.map(c => c[0]);
  assert.ok(aufrufe.includes('lineTo'), 'es muss gezeichnet werden');
  // Zwei Messwerte plus die Rohspur des Analogkanals
  assert.equal(K._elemente['chronik-legend'].children.length, 3);

  const eintrag = K._elemente['chronik-legend'].children[0];
  assert.equal(eintrag.dataset.spur, '0');
  assert.ok(eintrag.innerHTML.includes('Lichtstärke'));
});

test('ein Klick in der Legende blendet die Reihe aus und wieder ein', () => {
  const K = loadChronik();
  K.state.daten = K.parseStream(verkette(
    tabellenRahmen(1756612000, [
      { kanal: 0, analog: true, schluessel: 'ANALOG_0', name: 'Lichtstärke', einheit: '%' }
    ]),
    messRahmen(1756612060, [{ kanal: 0, wert: 30, roh: 600 }])
  ), null);
  K.state.grenzen = { von: 1756612000, bis: 1756612120 };
  K.state.view = { von: 1756612000, bis: 1756612120 };
  K.zeichne();

  const eintrag = K._elemente['chronik-legend'].children[0];
  eintrag.dispatch('click');
  assert.ok(K.state.versteckt.has('0'), 'nach dem Klick ist die Reihe aus');

  K._elemente['chronik-legend'].children[0].dispatch('click');
  assert.ok(!K.state.versteckt.has('0'), 'ein zweiter Klick holt sie zurück');
});

test('der Filter überlebt das Nachladen', () => {
  // Beim Auffrischen alle 60 s wird parseStream erneut aufgerufen - der
  // Zustand der Legende darf dabei nicht zurückspringen.
  const K = loadChronik();
  K.state.daten = K.parseStream(messRahmen(1756612060, [{ kanal: 0, wert: 30 }]), null);
  K.state.versteckt.add('0');
  K.state.daten = K.parseStream(messRahmen(1756612120, [{ kanal: 0, wert: 31 }]), K.state.daten);
  assert.ok(K.state.versteckt.has('0'));
  assert.equal(K.state.daten.reihen.get(0).t.length, 2);
});
