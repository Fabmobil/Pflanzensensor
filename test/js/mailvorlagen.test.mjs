/**
 * Tests für data/js/mailvorlagen.js - Prüfung und Vorschau der Mailvorlagen.
 *
 * FALLTABELLE - dieselben Nummern stehen in
 * test/test_mail_template/test_mail_template.cpp. Die Ersetzung gibt es
 * zweimal: hier für die Vorschau im Browser, dort für den Versand auf dem
 * Gerät. Läuft eine Seite aus dem Tritt, fällt es in einer der beiden Dateien
 * auf und nicht erst beim Empfänger der Mail.
 *
 *   1  Zeile ohne Platzhalter wird ein Absatz
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
 *  26  "# " am Zeilenanfang wird eine Überschrift
 *  27  **fett** wird ausgezeichnet
 *  28  Einzelne Sterne bleiben Text
 *  29  [Text](http://...) wird ein Link
 *  30  Andere Adressschemata werden nicht verlinkt
 *  31  Spitze Klammern werden maskiert - auch im eingesetzten Wert
 *  32  Leerzeile erzeugt keine Ausgabe
 *  33  Umbruch zerreißt weder Tag noch Entität
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

test('1: Zeile ohne Platzhalter wird ein Absatz', () => {
  assert.deepEqual(zeilen('Hallo Welt'), ['<p>Hallo Welt</p>']);
});

test('2: zwei Platzhalter in einer Zeile', () => {
  assert.deepEqual(zeilen('{geraet} auf {ip}'), ['<p>Frameclaw PS auf 172.17.1.44</p>']);
});

test('3: unbekannter Platzhalter bleibt wörtlich stehen', () => {
  // Der Nutzer soll seinen Tippfehler in der Vorschau sehen, nicht eine Lücke
  assert.deepEqual(zeilen('Wert: {quatsch}!'), ['<p>Wert: {quatsch}!</p>']);
});

test('4: {{ ergibt ein wörtliches {', () => {
  assert.deepEqual(zeilen('{{geraet} bleibt'), ['<p>{geraet} bleibt</p>']);
  assert.deepEqual(zeilen('a {{ b'), ['<p>a { b</p>']);
});

test('5: unabgeschlossene Klammer am Zeilenende', () => {
  assert.deepEqual(zeilen('Ende: {geraet'), ['<p>Ende: {geraet</p>']);
});

test('6: leerer Wert ergibt leeren Text', () => {
  // Die eckige Klammer leitet auch einen Link ein; ohne folgendes ( bleibt sie Text
  assert.deepEqual(zeilen('[{leer}]'), ['<p>[]</p>']);
});

test('7: Blockplatzhalter allein auf der Zeile', () => {
  assert.deepEqual(zeilen('{messwerte}'), BLOECKE.messwerte);
});

test('8: Blockplatzhalter mitten im Text bleibt wörtlich', () => {
  // Sonst müsste der Text davor und dahinter gepuffert werden - genau das,
  // was auf dem Gerät Speicher kostet
  assert.deepEqual(zeilen('Werte: {messwerte} soweit'), ['<p>Werte: {messwerte} soweit</p>']);
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
  // 399 Zeichen plus <p> und </p>
  assert.equal(raus.join('').length, 399 + 7);
});

test('12: Umbruch am letzten Leerzeichen', () => {
  const lang = 'x'.repeat(240) + ' ' + 'x'.repeat(108);
  const raus = zeilen(lang);
  assert.equal(raus.length, 2);
  // Das <p> davor verschiebt alles um drei Zeichen
  assert.equal(raus[0].length, 244);
  assert.equal(raus[0][243], ' ');
});

test('13: Umbruch zerreißt kein UTF-8-Zeichen', () => {
  // Emojis sind vier Byte lang; ein Schnitt mittendrin ergäbe ein Ersatzzeichen
  const raus = zeilen('🟢'.repeat(100));
  assert.ok(raus.length > 1);
  raus.forEach(z => {
    assert.ok(!z.includes('�'), 'kein Ersatzzeichen');
  });
});

test('14: eine Ersetzung, die die Zeile zu lang macht, bricht um', () => {
  const raus = M.expandiereZeile('Name: {geraet}', { geraet: 'N'.repeat(299) }, BLOECKE);
  assert.ok(raus.length > 1);
  [...raus].forEach(z => assert.ok(M.byteLaenge(z) <= M.ZEILE_MAX));
});

test('26: "# " am Zeilenanfang wird eine Überschrift', () => {
  assert.deepEqual(zeilen('# 🌻 Hallo {geraet}'), ['<h1>🌻 Hallo Frameclaw PS</h1>']);
  // Nur am Zeilenanfang und nur mit Leerzeichen dahinter
  assert.deepEqual(zeilen('Nr. # 1'), ['<p>Nr. # 1</p>']);
  assert.deepEqual(zeilen('#kein Leerzeichen'), ['<p>#kein Leerzeichen</p>']);
});

test('27: **fett** wird ausgezeichnet', () => {
  assert.deepEqual(zeilen('Bei **{geraet}** klemmt es'),
    ['<p>Bei <strong>Frameclaw PS</strong> klemmt es</p>']);
});

test('28: einzelne Sterne bleiben Text', () => {
  assert.deepEqual(zeilen('3 * 4 = 12'), ['<p>3 * 4 = 12</p>']);
  assert.deepEqual(zeilen('**ohne Ende'), ['<p>**ohne Ende</p>']);
});

test('29: [Text](http://...) wird ein Link', () => {
  assert.deepEqual(zeilen('[Jetzt nachschauen](http://{ip})'),
    ['<p><a href="http://172.17.1.44">Jetzt nachschauen</a></p>']);
});

test('30: andere Adressschemata werden nicht verlinkt', () => {
  // Aus einer Vorlage darf kein Mailprogramm etwas ausführen
  assert.deepEqual(zeilen('[Klick](javascript:alert(1))'), ['<p>[Klick](javascript:alert(1))</p>']);
  assert.deepEqual(zeilen('[Schreib](mailto:tommy@fabmobil.org)'),
    ['<p><a href="mailto:tommy@fabmobil.org">Schreib</a></p>']);
});

test('31: spitze Klammern werden maskiert', () => {
  assert.deepEqual(zeilen('5 < 7 & "eins"'), ['<p>5 &lt; 7 &amp; &quot;eins&quot;</p>']);
  // Auch der eingesetzte Wert - sonst wäre die eigene Vorlage ein Weg,
  // fremdes Markup in die Mail zu bekommen
  assert.deepEqual([...M.expandiereZeile('{geraet}', { geraet: '<script>x</script>' }, {})],
    ['<p>&lt;script&gt;x&lt;/script&gt;</p>']);
});

test('32: Leerzeile erzeugt keine Ausgabe', () => {
  assert.deepEqual(zeilen(''), []);
  assert.deepEqual(zeilen('   '), []);
});

test('33: Umbruch zerreißt weder Tag noch Entität', () => {
  // Jedes & wird zu fünf Zeichen, die Zeile bricht also mehrfach um
  const raus = zeilen('&'.repeat(119));
  assert.ok(raus.length > 1);
  raus.forEach(z => {
    const letztes = z.lastIndexOf('&');
    if (letztes >= 0) assert.ok(z.indexOf(';', letztes) >= 0, 'Entität nicht zerrissen');
    const auf = z.lastIndexOf('<');
    if (auf >= 0) assert.ok(z.indexOf('>', auf) >= 0, 'Tag nicht zerrissen');
  });
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
