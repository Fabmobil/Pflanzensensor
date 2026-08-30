/**
 * Tests für data/js/sensors.js - die Startseite.
 *
 * Zwei Dinge sind hier neu und ohne Abdeckung heikel: der Klick auf ein
 * Sensorblatt löst über den offenen Endpunkt POST /measure eine Messung aus,
 * und jedes Blatt wackelt, sobald eine neue Messung eintrifft. Beides wird
 * beim Ausprobieren am Gerät leicht falsch positiv: dass beim Laden der Seite
 * nicht alles auf einmal wackelt, dass ein zweiter Klick in der Sperre
 * wirklich nichts sendet und dass eine gescheiterte Anfrage kein ewig
 * wartendes Blatt hinterlässt, sieht man erst, wenn es zu spät ist.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadSensors, makeClock } from './helpers/load.mjs';

/** Zwei Messwerte desselben Sensors plus einer eines zweiten Sensors. */
const ZEILEN = [
  { key: 'DHT_0', sensorId: 'DHT' },
  { key: 'DHT_1', sensorId: 'DHT' },
  { key: 'ANALOG_1_0', sensorId: 'ANALOG_1' }
];

const messwert = (lastMeasurement, extra = {}) => ({
  value: 23.4, unit: '°C', status: 'green', measurementInterval: 60000, lastMeasurement, ...extra
});

/** Umgebung mit einer Antwort auf /measure; /getLatestValues liefert nichts Neues. */
function umgebung(measureAntwort = { ok: true, status: 200, body: { success: true } }) {
  const clock = makeClock();
  const S = loadSensors({
    rows: ZEILEN,
    clock,
    respond: (url) => {
      if (url === '/measure') {
        if (measureAntwort instanceof Error) throw measureAntwort;
        return measureAntwort;
      }
      return { ok: true, status: 200, body: { sensors: {} } };
    }
  });
  S.bindSensorTriggers();
  return { S, clock, zeile: (key) => S._document.querySelector(`[data-sensor="${key}"]`) };
}

test('erster Abruf wackelt nicht, merkt sich aber den Zeitstempel', () => {
  // Das serverseitig gebaute HTML liefert keinen Zeitstempel mit. Ohne diese
  // Bedingung wackelten beim Öffnen der Seite alle Blätter gleichzeitig.
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');

  S.updateSensorCard(el, messwert(5000));

  assert.equal(el.classList.contains('wiggle'), false);
  assert.equal(el.dataset.lastMeasurement, '5000');
});

test('unveränderter Zeitstempel wackelt nicht', () => {
  // Der Poller fragt alle 10 s; ohne Vergleich wackelte es dauerhaft weiter.
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');

  S.updateSensorCard(el, messwert(5000));
  S.updateSensorCard(el, messwert(5000));

  assert.equal(el.classList.contains('wiggle'), false);
});

test('neuer Zeitstempel wackelt und beendet den Wartezustand', () => {
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');
  S.updateSensorCard(el, messwert(5000));

  el.classList.add('measuring');
  el.dataset.measuringSince = '1';
  S.updateSensorCard(el, messwert(9000));

  assert.equal(el.classList.contains('wiggle'), true);
  assert.equal(el.classList.contains('measuring'), false);
  assert.equal(el.dataset.measuringSince, undefined);
});

test('Zeitstempel rückwärts gilt auch als neue Messung', () => {
  // Nach einem Neustart des Geräts fängt millis() wieder bei null an.
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');

  S.updateSensorCard(el, messwert(500000));
  S.updateSensorCard(el, messwert(1200));

  assert.equal(el.classList.contains('wiggle'), true);
});

test('Klick sendet die Sensor-ID und lässt alle Zeilen des Sensors warten', async () => {
  const { S, zeile } = umgebung();

  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  const anMessen = S._fetches.filter((f) => f.url === '/measure');
  assert.equal(anMessen.length, 1);
  assert.equal(anMessen[0].options.method, 'POST');
  // Der Schlüssel wäre DHT_0 - gebraucht wird die reine ID DHT.
  assert.equal(anMessen[0].options.body, 'sensor=DHT');
  assert.equal(zeile('DHT_0').classList.contains('measuring'), true);
  assert.equal(zeile('DHT_1').classList.contains('measuring'), true);
  // Der andere Sensor misst nicht mit.
  assert.equal(zeile('ANALOG_1_0').classList.contains('measuring'), false);
});

test('Klick auf das Blatt zählt als Klick auf die Zeile', async () => {
  // Der Zuhörer sitzt am Container; getroffen wird meist ein Kindelement.
  const { S, zeile } = umgebung();
  const blatt = zeile('ANALOG_1_0').querySelector('.leaf');

  blatt.dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 1);
});

test('zweiter Klick innerhalb der Sperre sendet nichts', async () => {
  const { S, clock, zeile } = umgebung();

  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));
  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 1);

  // Nach Ablauf der Sperre geht es wieder - der Wartezustand muss dafür weg sein.
  S.updateSensorCard(zeile('DHT_0'), messwert(1000));
  S.updateSensorCard(zeile('DHT_0'), messwert(2000));
  clock.advance(4000);
  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 2);
});

test('429 räumt den Wartezustand ab und wartet die genannte Zeit', async () => {
  const { S, clock, zeile } = umgebung({
    ok: false, status: 429, body: { success: false, retryAfterMs: 2000 }
  });

  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(zeile('DHT_0').classList.contains('measuring'), false);
  assert.equal(zeile('DHT_1').classList.contains('measuring'), false);
  assert.match(zeile('DHT_0').querySelector('.interval span').textContent, /warten/);

  clock.advance(1000); // noch gesperrt
  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));
  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 1);

  clock.advance(1500); // Sperre abgelaufen
  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));
  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 2);
});

test('Netzwerkfehler hinterlässt keine wartende Zeile', async () => {
  const { zeile } = umgebung(new Error('nicht erreichbar'));

  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(zeile('DHT_0').classList.contains('measuring'), false);
  assert.match(zeile('DHT_0').querySelector('.interval span').textContent, /Fehler/);
});

test('Wartezustand wird nach der Frist von allein aufgelöst', async () => {
  const { S, clock, zeile } = umgebung();

  zeile('DHT_0').dispatch('click');
  await new Promise((r) => setTimeout(r, 10));
  assert.equal(zeile('DHT_0').classList.contains('measuring'), true);

  clock.advance(30000); // länger als MEASURING_TIMEOUT_MS
  S.releaseStaleMeasuring();

  assert.equal(zeile('DHT_0').classList.contains('measuring'), false);
});

test('der Sekundentakt überschreibt den Wartehinweis nicht', () => {
  // Ohne diese Ausnahme stünde eine Sekunde später wieder der Countdown da.
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');
  S.updateSensorCard(el, messwert(1000));

  el.classList.add('measuring');
  el.dataset.measuringSince = String(Date.now());
  el.querySelector('.interval span').textContent = '(misst …)';
  S.updateCountdowns();

  assert.equal(el.querySelector('.interval span').textContent, '(misst …)');
});

test('Statuswechsel von warmup entfernt die alte Klasse', () => {
  // 'warmup' fehlte in beiden remove()-Listen: die Klasse blieb kleben und
  // färbte Text und Blatt danach weiter falsch.
  const { S, zeile } = umgebung();
  const el = zeile('DHT_0');

  S.updateSensorCard(el, messwert(1000, { status: 'warmup' }));
  assert.equal(el.classList.contains('sensor-status-warmup'), true);

  S.updateSensorCard(el, messwert(2000, { status: 'green' }));
  assert.equal(el.classList.contains('sensor-status-warmup'), false);
  assert.equal(el.classList.contains('sensor-status-green'), true);
  assert.equal(el.querySelector('.status').classList.contains('warmup'), false);
});

test('ohne data-sensor-id wird der Messindex abgeschnitten', () => {
  // Rückfall für Geräte mit älterer Firmware, die das Attribut nicht liefert.
  const S = loadSensors({ rows: [{ key: 'ANALOG_1_0', sensorId: null }] });
  const el = S._document.querySelector('[data-sensor="ANALOG_1_0"]');

  assert.equal(S.sensorIdOf(el), 'ANALOG_1');
});

test('Leertaste löst aus und unterdrückt das Scrollen', async () => {
  const { S, zeile } = umgebung();

  const event = zeile('DHT_0').dispatch('keydown', { key: ' ' });
  await new Promise((r) => setTimeout(r, 10));

  assert.equal(event.defaultPrevented, true);
  assert.equal(S._fetches.filter((f) => f.url === '/measure').length, 1);
});
