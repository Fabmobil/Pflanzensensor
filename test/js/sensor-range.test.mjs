/**
 * Tests für ThresholdSliderInitializer.calculateRange() aus admin_sensors.js.
 *
 * Die Funktion bestimmt die Skala der Schwellwert-Regler. Liefert sie einen
 * falschen Bereich, stehen die Regler an der falschen Stelle und die
 * Kalibrierung wird still verfälscht - es gibt keine Fehlermeldung, nur
 * Messwerte, die nicht mehr zu den Schwellen passen. Sechs Verzweigungen
 * (Analog, Prozent, Temperatur, CO2, Feinstaub, allgemein), Pufferrechnung um
 * die Schwellen herum und ein minmax-Vorrang - bisher ohne jede Abdeckung.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadAdminSensors } from './helpers/load.mjs';

const { ThresholdSliderInitializer: TSI } = loadAdminSensors();
const range = (sensor, meas, index = 0) => TSI.calculateRange(sensor, meas, index);

/** Schwellen in der Kurzschreibweise, die das Gerät liefert. */
const thresh = (yl, gl, gh, yh) => ({ thresh: { yl, gl, gh, yh } });

test('Analogsensoren bekommen immer die feste Skala 0-100 %', () => {
  // Analogwerte sind relativ; die Rohwerte haben eine ganz andere Größe.
  const r = range({ id: 'ANALOG_0' }, { nm: 'Bodenfeuchte', un: '%', ...thresh(20, 40, 60, 80) });
  assert.equal(r.min, 0);
  assert.equal(r.max, 100);
});

test('Analogskala weitet sich, wenn die Schwellen nicht hineinpassen', () => {
  // Die 0-100-Skala ist der Normalfall, aber keine harte Grenze: liegen die
  // Schwellen darueber, waeren sie sonst am Regler nicht erreichbar.
  const r = range({ id: 'ANALOG_3' }, { nm: 'Rohwert', un: '', ...thresh(200, 400, 600, 800) });
  assert.equal(r.min, 0);
  assert.ok(r.max >= 800, `max ${r.max} muss die oberste Schwelle einschliessen`);
});

test('Feuchte wird an der Einheit % erkannt, nicht nur am Namen', () => {
  const r = range({ id: 'DHT' }, { nm: 'Luftfeuchte', un: '%', ...thresh(30, 40, 60, 70) });
  assert.equal(r.min, 0);
  assert.equal(r.max, 100);
});

test('Temperatur reicht nach oben mindestens bis 50 und nie unter -40 Grad', () => {
  // Math.max(min, -40) ist eine UNTERGRENZE, kein Zielwert: die Skala faengt
  // eng um die Schwellen an und wird nur daran gehindert, unter -40 zu rutschen.
  // Nach oben sorgt Math.max(max, 50) dafuer, dass Sommerwerte hineinpassen.
  const r = range({ id: 'DHT' }, { nm: 'Lufttemperatur', un: '°C', ...thresh(18, 20, 24, 28) });
  assert.ok(r.min >= -40, `min ${r.min} darf nicht unter -40 liegen`);
  assert.ok(r.min < 18, `min ${r.min} muss unter der kleinsten Schwelle liegen`);
  assert.ok(r.max >= 50, `max ${r.max} muss mindestens 50 sein`);
});

test('CO2 wird nie negativ', () => {
  const r = range({ id: 'MHZ19' }, { nm: 'CO2', un: 'ppm', ...thresh(400, 600, 1000, 1400) });
  assert.ok(r.min >= 0, `min ${r.min} darf nicht negativ sein`);
  assert.ok(r.max >= 1400, 'oberste Schwelle muss in die Skala passen');
});

test('Feinstaub wird nie negativ', () => {
  const r = range({ id: 'SDS011' }, { nm: 'PM2.5', un: 'µg/m³', ...thresh(5, 10, 20, 35) });
  assert.ok(r.min >= 0, `min ${r.min} darf nicht negativ sein`);
});

test('allgemeiner Fall legt einen Puffer um die Schwellen', () => {
  // 20 % des Schwellenbereichs als Luft nach oben und unten, damit die
  // aeusseren Schwellen nicht am Reglerrand kleben.
  const r = range({ id: 'HX711' }, { nm: 'Gewicht', un: 'g', ...thresh(100, 200, 300, 400) });
  assert.ok(r.min < 100, `min ${r.min} muss unter der kleinsten Schwelle liegen`);
  assert.ok(r.max > 400, `max ${r.max} muss ueber der groessten Schwelle liegen`);
});

test('minmax aus der Konfiguration hat Vorrang vor der Pufferrechnung', () => {
  const r = range({ id: 'HX711' }, {
    nm: 'Gewicht', un: 'g', ...thresh(100, 200, 300, 400),
    minmax: { min: 0, max: 1000 }
  });
  assert.equal(r.min, 0);
  assert.equal(r.max, 1000);
});

test('minmax greift NICHT bei Analogsensoren - die bleiben bei 0-100', () => {
  const r = range({ id: 'ANALOG_1' }, {
    nm: 'Bodenfeuchte', un: '%', ...thresh(20, 40, 60, 80),
    minmax: { min: 0, max: 1023 }
  });
  assert.equal(r.max, 100, 'Analogskala darf nicht von minmax ueberschrieben werden');
});

test('lange Feldnamen werden wie die Kurzform gelesen', () => {
  // Das Gerät schickt je nach Pfad thresh/yl oder thresholds/yellowLow.
  const kurz = range({ id: 'HX711' }, { nm: 'Gewicht', un: 'g', ...thresh(100, 200, 300, 400) });
  const lang = range({ id: 'HX711' }, {
    name: 'Gewicht', unit: 'g',
    thresholds: { yellowLow: 100, greenLow: 200, greenHigh: 300, yellowHigh: 400 }
  });
  assert.deepEqual(lang, kurz);
});

test('gleiche Schwellen ergeben trotzdem einen Bereich mit Breite', () => {
  // Regressionstest: vorher kam hier min == max heraus. toPercent() teilt
  // durch (max - min) und lieferte NaN bzw. Infinity - die Marken des Reglers
  // verschwanden oder klebten am Rand.
  const r = range({ id: 'HX711' }, { nm: 'Gewicht', un: 'g', ...thresh(5, 5, 5, 5) });
  assert.ok(r.max > r.min, `Bereich muss Breite haben, war ${r.min}..${r.max}`);
});

test('liefert immer ein brauchbares Intervall (min < max)', () => {
  const faelle = [
    [{ id: 'ANALOG_0' }, { nm: 'x', un: '%', ...thresh(0, 0, 0, 0) }],
    [{ id: 'DHT' }, { nm: 'Lufttemperatur', un: '°C', ...thresh(20, 20, 20, 20) }],
    [{ id: 'HX711' }, { nm: 'Gewicht', un: 'g', ...thresh(5, 5, 5, 5) }]
  ];
  for (const [sensor, meas] of faelle) {
    const r = range(sensor, meas);
    assert.ok(isFinite(r.min) && isFinite(r.max), `endliche Grenzen für ${meas.nm}`);
    assert.ok(r.min < r.max, `min<max für ${meas.nm}, war ${r.min}..${r.max}`);
  }
});

/**
 * Der Poller fragt alle 5 s /getLatestValues ab. Der Sensor beantwortet nur
 * eine Verbindung zugleich und baut dafür das komplette JSON aller Sensoren
 * auf - ein vergessener Hintergrundtab wäre Dauerlast ohne Zuschauer.
 */
test('Poller fragt im verborgenen Tab nicht ab', async () => {
  // Getrieben über start(), das intern sofort einmal update() aufruft -
  // SensorUpdater exportiert nur start/stop, und dafür allein die Schnittstelle
  // zu erweitern waere die falsche Reihenfolge.
  const A = loadAdminSensors();
  const doc = A._document;

  doc.visibilityState = 'hidden';
  A.SensorUpdater.start();
  assert.equal(A._fetches.length, 0, 'im verborgenen Tab darf nichts abgefragt werden');
  A.SensorUpdater.stop();

  doc.visibilityState = 'visible';
  A.SensorUpdater.start();
  assert.equal(A._fetches.length, 1, 'sichtbar wird wieder abgefragt');
  assert.match(A._fetches[0], /getLatestValues/);
  A.SensorUpdater.stop();
});
