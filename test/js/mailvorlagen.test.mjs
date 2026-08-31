/**
 * Tests für data/js/mailvorlagen.js - Prüfung und Vorschau der Mailvorlagen.
 *
 * FALLTABELLE - dieselben Nummern stehen in
 * test/test_mail_template/test_mail_template.cpp. Die Ersetzung gibt es
 * zweimal: hier für die Vorschau im Browser, dort für den Versand auf dem
 * Gerät. Läuft eine Seite aus dem Tritt, fällt es in einer der beiden Dateien
 * auf und nicht erst beim Empfänger der Mail.
 *
 *   1  Zeile ohne Platzhalter bleibt unverändert
 *   2  Zwei Platzhalter in einer Zeile
 *   3  Unbekannter Platzhalter bleibt wörtlich stehen
 *   4  {{ ergibt ein wörtliches {
 *   5  Unabgeschlossene Klammer am Zeilenende bleibt stehen
 *   6  Leerer Wert ergibt leeren Text, nicht den Platzhalternamen
 *   7  Blockplatzhalter allein auf der Zeile erzeugt mehrere Zeilen
 *   8  Blockplatzhalter mitten im Text bleibt wörtlich
 *   9  Leerraum um den Blockplatzhalter ist erlaubt
 *  10  Block ohne Zeilen erzeugt keine Ausgabe
 *  11  Zu lange Zeile wird umgebrochen
 *  12  Umbruch am letzten Leerzeichen
 *  13  Umbruch zerreißt kein UTF-8-Zeichen
 *  14  Eine Ersetzung, die die Zeile zu lang macht, bricht um
 *  18  Rundlauf der Entwertung: [boot.rumpf]
 *  19  Rundlauf der Entwertung: \x
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadMailVorlagen } from './helpers/load.mjs';

const M = loadMailVorlagen();

const WERTE = {
  geraet: 'Frameclaw PS',
  ip: '172.17.1.44',
  ssid: 'Magrathea',
  neustarts: '7',
  laufzeit: '2d 5h',
  datum: '31.08.2026',
  uhrzeit: '18:24',
  leer: ''
};

const BLOECKE = {
  messwerte: ['<tr>messwerte 1</tr>', '<tr>messwerte 2</tr>', '<tr>messwerte 3</tr>'],
  auffaellige: ['<tr>auffaellige 1</tr>', '<tr>auffaellige 2</tr>', '<tr>auffaellige 3</tr>']
};

const zeilen = (text, bloecke = BLOECKE) => [...M.expandiereZeile(text, WERTE, bloecke)];

test('1: Zeile ohne Platzhalter bleibt unverändert', () => {
  assert.deepEqual(zeilen('<p>Hallo Welt</p>'), ['<p>Hallo Welt</p>']);
});

test('2: zwei Platzhalter in einer Zeile', () => {
  assert.deepEqual(zeilen('{geraet} auf {ip}'), ['Frameclaw PS auf 172.17.1.44']);
});

test('3: unbekannter Platzhalter bleibt wörtlich stehen', () => {
  // Der Nutzer soll seinen Tippfehler in der Vorschau sehen, nicht eine Lücke
  assert.deepEqual(zeilen('Wert: {quatsch}!'), ['Wert: {quatsch}!']);
});

test('4: {{ ergibt ein wörtliches {', () => {
  assert.deepEqual(zeilen('{{geraet} bleibt'), ['{geraet} bleibt']);
  assert.deepEqual(zeilen('a {{ b'), ['a { b']);
});

test('5: unabgeschlossene Klammer am Zeilenende', () => {
  assert.deepEqual(zeilen('Ende: {geraet'), ['Ende: {geraet']);
});

test('6: leerer Wert ergibt leeren Text', () => {
  assert.deepEqual(zeilen('[{leer}]'), ['[]']);
});

test('7: Blockplatzhalter allein auf der Zeile', () => {
  assert.deepEqual(zeilen('{messwerte}'), BLOECKE.messwerte);
});

test('8: Blockplatzhalter mitten im Text bleibt wörtlich', () => {
  // Sonst müsste der Text davor und dahinter gepuffert werden - genau das,
  // was auf dem Gerät Speicher kostet
  assert.deepEqual(zeilen('<td>{messwerte}</td>'), ['<td>{messwerte}</td>']);
});

test('9: Leerraum um den Blockplatzhalter ist erlaubt', () => {
  assert.deepEqual(zeilen('   {auffaellige}  '), BLOECKE.auffaellige);
});

test('10: Block ohne Zeilen erzeugt keine Ausgabe', () => {
  assert.deepEqual(zeilen('{messwerte}', { messwerte: [], auffaellige: [] }), []);
});

test('11: zu lange Zeile wird umgebrochen', () => {
  const lang = 'a'.repeat(399);
  const raus = zeilen(lang);
  assert.equal(raus.length, 2);
  raus.forEach(z => assert.ok(M.byteLaenge(z) <= M.ZEILE_MAX));
  assert.equal(raus.join('').length, 399);
});

test('12: Umbruch am letzten Leerzeichen', () => {
  const lang = 'x'.repeat(240) + ' ' + 'x'.repeat(108);
  const raus = zeilen(lang);
  assert.equal(raus.length, 2);
  assert.equal(raus[0].length, 241);
  assert.equal(raus[0][240], ' ');
});

test('13: Umbruch zerreißt kein UTF-8-Zeichen', () => {
  // Emojis sind vier Byte lang; ein Schnitt mittendrin ergäbe ein Ersatzzeichen
  const raus = zeilen('🟢'.repeat(100));
  assert.ok(raus.length > 1);
  raus.forEach(z => {
    assert.ok(!z.includes('�'), 'kein Ersatzzeichen');
    assert.equal(M.byteLaenge(z) % 4, 0);
  });
});

test('14: eine Ersetzung, die die Zeile zu lang macht, bricht um', () => {
  const raus = M.expandiereZeile('Name: {geraet}', { geraet: 'N'.repeat(299) }, BLOECKE);
  assert.ok(raus.length > 1);
  [...raus].forEach(z => assert.ok(M.byteLaenge(z) <= M.ZEILE_MAX));
});

test('18, 19: Rundlauf der Entwertung', () => {
  assert.equal(M.entwerte('\\[boot.rumpf]'), '[boot.rumpf]');
  assert.equal(M.entwerte('\\\\x'), '\\x');
  assert.equal(M.entwerte('<html>'), '<html>');

  assert.ok(M.istMarke('[boot.rumpf]'));
  assert.ok(M.istMarke('[alive.betreff]'));
  assert.ok(!M.istMarke('<p>[1]</p>'));
  assert.ok(!M.istMarke('[Boot.Rumpf]'));
  assert.ok(!M.istMarke('[boot.rumpf] '));
});

// === Bytezählung ===

test('byteLaenge zählt Bytes, nicht Zeichen', () => {
  assert.equal(M.byteLaenge('a'), 1);
  assert.equal(M.byteLaenge('ä'), 2);
  assert.equal(M.byteLaenge('€'), 3);
  assert.equal(M.byteLaenge('🟢'), 4);
  assert.equal(M.byteLaenge(''), 0);
});

test('die Bytegrenze greift auch bei lauter Emojis', () => {
  // 500 Emojis sind 2000 Bytes, aber nur 1000 UTF-16-Einheiten - das Attribut
  // maxlength im Browser würde sie durchlassen
  const text = '🟢'.repeat(500);
  assert.equal(text.length, 1000);
  const ergebnis = M.pruefe(text, M.RUMPF_MAX);
  assert.equal(ergebnis.bytes, 2000);
  assert.ok(ergebnis.befunde.some(b => b.includes('Zu lang')));
});

// === Prüfung ===

test('pruefe meldet unbekannte Platzhalter mit Zeilennummer', () => {
  const ergebnis = M.pruefe('<p>{geraet}</p>\n<p>{quatsch}</p>');
  assert.equal(ergebnis.befunde.length, 1);
  assert.ok(ergebnis.befunde[0].includes('Zeile 2'));
  assert.ok(ergebnis.befunde[0].includes('{quatsch}'));
});

test('pruefe meldet einen Block mitten in der Zeile', () => {
  const ergebnis = M.pruefe('<td>{messwerte}</td>');
  assert.equal(ergebnis.befunde.length, 1);
  assert.ok(ergebnis.befunde[0].includes('allein in einer Zeile'));

  // Allein auf der Zeile ist es in Ordnung
  assert.equal(M.pruefe('{messwerte}').befunde.length, 0);
});

test('pruefe meldet eine zu lange Zeile', () => {
  // Solche Zeilen liest das Gerät beim nächsten Öffnen in Stücken, und aus
  // einer würden beim erneuten Speichern dauerhaft mehrere
  const ergebnis = M.pruefe('<p>ok</p>\n' + 'x'.repeat(300));
  assert.ok(ergebnis.befunde.some(b => b.includes('Zeile 2') && b.includes('zu lang')));

  assert.equal(M.pruefe('x'.repeat(256)).befunde.length, 0);
  // In Bytes gerechnet, nicht in Zeichen: 65 Emojis sind 260 Bytes
  assert.ok(M.pruefe('🌻'.repeat(65)).befunde.some(b => b.includes('zu lang')));
});

test('pruefe warnt bei einer Zeile, die wie eine Abschnittsmarke aussieht', () => {
  const ergebnis = M.pruefe('<p>ok</p>\n[alive.rumpf]');
  assert.ok(ergebnis.befunde.some(b => b.includes('Zeile 2') && b.includes('Abschnittsmarke')));
});

test('eine saubere Vorlage ergibt keine Befunde', () => {
  const ergebnis = M.pruefe('<p>{geraet} auf {ip}, {ssid}</p>\n{messwerte}\n<p>{neustarts}</p>');
  assert.deepEqual([...ergebnis.befunde], []);
});

// === Vorschau ===

test('die Vorschau ersetzt alle bekannten Platzhalter', () => {
  const text = '<p>{geraet} / {ip} / {ssid} / {neustarts} / {laufzeit} / {datum} / {uhrzeit}</p>\n{messwerte}';
  const raus = M.vorschau(text);
  M.BEKANNT.forEach(name => {
    assert.ok(!raus.includes('{' + name + '}'), `${name} steht noch roh drin`);
  });
  assert.ok(raus.includes('Frameclaw PS'));
  assert.ok(raus.includes('<tr>'), 'die Beispieltabelle wird eingesetzt');
});

test('die Vorschau lässt unbekannte Platzhalter stehen', () => {
  assert.ok(M.vorschau('<p>{quatsch}</p>').includes('{quatsch}'));
});
