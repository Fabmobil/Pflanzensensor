/** Verhalten der Admin-Seiten mit echten HTML-Dateien und Backend-Fixtures */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadRealPage, settle, apiResponder, fixture } from './dom.mjs';
import { makeClock } from './load.mjs';

function open(page, overrides = {}, opts = {}) {
  const clock = makeClock(1000);
  const p = loadRealPage(page, { respond: apiResponder(overrides), clock, ...opts });
  p.start();
  return p;
}
const writes = (p, method) => p.requests.filter((r) => r.method === method);
const body = (r) => JSON.parse(r.body);
function type(el, value) {
  el.value = value;
  el.dispatch(el.type === 'checkbox' || el.tagName === 'SELECT' ? 'change' : 'input');
}
function check(el, on) {
  el.checked = on;
  el.dispatch('change');
}
const toasts = (p) => p.document.querySelectorAll('.toast').map((t) => t.textContent);

// ---------------------------------------------------------------------------
test('Einstellungen: Felder aus der Konfiguration, Autosave je Feld', async () => {
  const p = open('admin.html');
  await settle(40);
  const d = p.document;
  const name = d.getElementById('deviceName');
  assert.equal(name.value, 'Balkon');
  assert.equal(d.getElementById('hostPreview').textContent, 'balkon');
  type(name, 'Küche Süd');
  assert.equal(d.getElementById('hostPreview').textContent, 'kueche-sued');
  const level = d.getElementById('logLevel');
  assert.equal(level.value, 'INFO');
  type(level, 'DEBUG');
  p.clock.advance(1100);
  await settle();
  const patches = writes(p, 'PATCH').map(body);
  assert.deepEqual(patches, [{ general: { deviceName: 'Küche Süd' } }, { log: { level: 'DEBUG' } }]);
  assert.ok(toasts(p).includes('Gespeichert'));
});

test('Einstellungen: Systeminformation aus /api/v1/config', async () => {
  const p = open('admin.html');
  await settle(40);
  const t = p.document.getElementById('sysinfo').textContent;
  assert.match(t, /Freier Heap:29\.3 KB/);
  assert.match(t, /MAC:AA:BB:CC:DD:EE:FF/);
  assert.match(t, /Log-Datei:0 B \(0 % von max\.\)/);
});

test('Einstellungen: aktiver WLAN-Slot gesperrt, andere speicherbar', async () => {
  const p = open('admin.html');
  await settle(40);
  const slots = p.document.getElementById('wifiSlots');
  const inputs = slots.querySelectorAll('input');
  assert.equal(inputs.length, 9);
  assert.equal(inputs[0].value, 'Heimnetz');
  assert.ok(inputs[0].disabled, 'aktiver Slot nicht änderbar');
  assert.equal(inputs[1].placeholder, '(gespeichert)');
  assert.match(slots.querySelector('h3').textContent, /aktiv/);
  // Slot 2 speichern mit Reconnect
  inputs[3].value = 'Gast';
  inputs[4].value = 'geheim';
  p.document.getElementById('wifiReconnect').checked = true;
  slots.querySelectorAll('button')[1].dispatch('click');
  await settle();
  const put = writes(p, 'PUT')[0];
  assert.equal(put.url, '/api/v1/secrets/wifi/2');
  assert.deepEqual(body(put), { ssid: 'Gast', password: 'geheim', clear: false, reconnect: true });
  assert.equal(inputs[4].value, '', 'Passwortfeld wird geleert');
});

test('Einstellungen: Passwort prüfen vor dem Senden', async () => {
  const p = open('admin.html');
  await settle(40);
  const d = p.document;
  d.getElementById('pw1').value = 'abc';
  d.getElementById('pw2').value = 'abd';
  d.getElementById('pwSave').dispatch('click');
  await settle();
  assert.equal(writes(p, 'PUT').length, 0);
  assert.ok(toasts(p).some((t) => /stimmen nicht/.test(t)));
  d.getElementById('pw2').value = 'abc';
  d.getElementById('pwSave').dispatch('click');
  await settle();
  assert.deepEqual(body(writes(p, 'PUT')[0]), { password: 'abc' });
  assert.equal(d.getElementById('pw1').value, '');
});

test('Einstellungen: Neustart und Werksreset gehen über DeviceWait; Abbrechen sendet nichts', async () => {
  let ok = false;
  const p = open('admin.html', { '/status': { body: { uptime: 10, bootId: 1 } } }, { extra: { confirm: () => ok } });
  await settle(40);
  p.document.getElementById('reset').dispatch('click');
  await settle(40);
  assert.equal(writes(p, 'POST').length, 0);
  ok = true;
  p.document.getElementById('reboot').dispatch('click');
  await settle(40);
  assert.equal(writes(p, 'POST')[0].url, '/api/v1/system/reboot');
  assert.ok(p.document.querySelector('.devicewait-overlay'), 'Overlay sichtbar');
});

test('Einstellungen: Import ohne Datei meldet Fehler', async () => {
  const p = open('admin.html');
  await settle(40);
  p.document.getElementById('importBtn').dispatch('click');
  await settle();
  assert.ok(toasts(p).some((t) => /Datei wählen/.test(t)));
});

// ---------------------------------------------------------------------------
test('Sensoren: Karten je Sensor, Kanalfelder speichern als Teil-JSON', async () => {
  const p = open('sensors.html');
  await settle(40);
  const d = p.document;
  const cards = d.querySelectorAll('.sensor-card');
  assert.equal(cards.length, fixture('sensors.json').sensors.length);
  const analog = d.querySelector('[data-sensor="ANALOG"]');
  const ch1 = analog.querySelector('[data-key="ANALOG_1"]');
  const name = ch1.querySelector('.name-row input[type="text"]');
  assert.equal(name.value, 'Bodenfeuchte');
  type(name, 'Topf links');
  const inv = ch1.querySelectorAll('.inverted-section input')[0];
  assert.ok(inv.checked);
  check(inv, false);
  p.clock.advance(1100);
  await settle();
  const patches = writes(p, 'PATCH').map(body);
  assert.deepEqual(patches, [
    { ANALOG: { channels: { 1: { name: 'Topf links' } } } },
    { ANALOG: { channels: { 1: { inverted: false } } } }
  ]);
});

test('Sensoren: Werte, Status, Extrema und Rohwerte werden angezeigt', async () => {
  const p = open('sensors.html');
  await settle(40);
  const d = p.document;
  const ch = d.querySelector('[data-key="ANALOG_0"]');
  const ro = ch.querySelectorAll('.readonly-value').map((e) => e.textContent);
  assert.ok(ro[0].includes('(OK)') || ro[0].includes('('), ro[0]);
  assert.match(ro[2], /^\d+$/, 'letzter Rohwert');
  assert.match(d.querySelector('[data-status="MHZ19"]').textContent, /Aufwärmen/);
  assert.equal(d.querySelector('[data-status="DHT"]').textContent, 'OK');
  // HX711 hat einen Tara-Knopf
  const tare = d.querySelector('[data-sensor="HX711"]').querySelectorAll('button').find((b) => b.textContent === 'Tarieren');
  tare.dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/HX711/tare');
});

const button = (root, text) => root.querySelectorAll('button').find((b) => b.textContent === text);

test('Sensoren: Waage kalibrieren, Wasservorrat, Kalibrierpunkte', async () => {
  const p = open('sensors.html');
  await settle(40);
  const d = p.document;
  const hx = d.querySelector('[data-sensor="HX711"]');
  button(hx, '1. Ausgangswert merken').dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/HX711/scale-reference');
  // Ohne Gramm wird nichts geschickt
  const n = writes(p, 'POST').length;
  button(hx, '2. Kalibrieren').dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').length, n);
  const grams = hx.querySelectorAll('input').find((i) => i.getAttribute('aria-label') === 'Referenzgewicht in Gramm');
  grams.value = '1000';
  button(hx, '2. Kalibrieren').dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/HX711/scale');
  assert.deepEqual(body(writes(p, 'POST').at(-1)), { grams: 1000 });
  // Skalierung und Nassgewicht speichern als Sensorfelder
  const field = (label) => hx.querySelectorAll('input').find((i) => i.getAttribute('aria-label') === label);
  assert.equal(field('Rohwert je Gramm').value, '-7050');
  type(field('Rohwert je Gramm'), '-98.2');
  type(field('Nassgewicht in Gramm'), '3100');
  p.clock.advance(1100);
  await settle();
  assert.deepEqual(writes(p, 'PATCH').map(body), [{ HX711: { scale: -98.2 } }, { HX711: { wet: 3100 } }]);
  assert.match(hx.querySelector('.watering-status').textContent, /nicht erkannt/);
  // Wasservorrat: aktuelles Gewicht als 100 %
  const water = hx.querySelector('[data-key="HX711_1"]');
  button(water, 'Jetzt = 100 %').dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/HX711/1/calibrate');
  assert.deepEqual(body(writes(p, 'POST').at(-1)), { percent: 100 });
  // Gewicht selbst hat keine Kalibrierpunkte, DHT auch nicht
  assert.equal(button(hx.querySelector('[data-key="HX711_0"]'), 'Jetzt = 0 %'), undefined);
  assert.equal(d.querySelector('[data-key="DHT_0"]').querySelector('.calibration-section'), null);
  // Analog: Rohwert als 0 %, Lernen einschalten
  const soil = d.querySelector('[data-key="ANALOG_1"]');
  button(soil, 'Jetzt = 0 %').dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/ANALOG/1/calibrate');
  assert.deepEqual(body(writes(p, 'POST').at(-1)), { percent: 0 });
  const learn = soil.querySelector('.calibration-section input[type="checkbox"]');
  check(learn, true);
  p.clock.advance(10);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH').at(-1)), { ANALOG: { channels: { 1: { learn: true } } } });
  assert.ok(button(soil, 'Vorschlag übernehmen').disabled, 'ohne Vorschlag gesperrt');
});

test('Sensoren: Werte vom Gerät ziehen nach, nicht beim Tippen', async () => {
  const later = structuredClone(fixture('sensors.json'));
  const an = later.sensors.find((s) => s.id === 'ANALOG').channels[1];
  an.rawMin = 310; // Kalibrierpunkt gesetzt
  an.fit = { bins: [null, null, null, { n: 5, raw: 588 }, null, { n: 9, raw: 500 }, null, { n: 4, raw: 412 }, null, null],
    suggest: { rawMin: 280, rawMax: 720, inverted: true, r2: 0.991, bins: 3 } };
  const hxLater = later.sensors.find((s) => s.id === 'HX711');
  hxLater.wet = 3150; // nach dem Gießen nachgeführt
  hxLater.lastWatering = { settling: true, at: later.now - 10 * 60000, rise: 700 };
  let calls = 0;
  const p = open('sensors.html', { 'GET /api/v1/sensors': () => ({ body: calls++ ? later : fixture('sensors.json') }) });
  await settle(40);
  const d = p.document;
  const soil = d.querySelector('[data-key="ANALOG_1"]');
  const [, rmin, rmax] = soil.querySelectorAll('.inverted-section input');
  type(rmax, '800'); // tippt gerade, Gerät meldet noch 750
  p.ctx._intervals.forEach((i) => i.fn()); // 5-s-Auffrischung (und Fußzeile)
  await settle(20);
  assert.equal(rmin.value, '310');
  assert.equal(rmax.value, '800', 'Eingabe bleibt');
  const hx = d.querySelector('[data-sensor="HX711"]');
  const wet = hx.querySelectorAll('input').find((i) => i.getAttribute('aria-label') === 'Nassgewicht in Gramm');
  assert.equal(wet.value, '3150');
  assert.match(hx.querySelector('.watering-status').textContent, /vor 10 min \(\+700 g\), tropft ab/);
  assert.match(soil.querySelector('.fit-suggestion').textContent, /280 bis 720, invertiert \(R² 0\.99 aus 3 Bereichen\)/);
  assert.equal(soil.querySelectorAll('.fit-bin').length, 3);
  const apply = button(soil, 'Vorschlag übernehmen');
  assert.ok(!apply.disabled);
  apply.dispatch('click');
  await settle();
  assert.equal(writes(p, 'POST').at(-1).url, '/api/v1/sensors/ANALOG/1/fit-apply');
  // Nachziehen speichert nichts zurück, nur die eigene Eingabe geht raus
  p.clock.advance(1100);
  await settle();
  assert.deepEqual(writes(p, 'PATCH').map(body), [{ ANALOG: { channels: { 1: { rawMax: 800 } } } }]);
});

test('Sensoren: Autokalibrierung sperrt die Rohgrenzen, Schwellen sind geordnet', async () => {
  const p = open('sensors.html');
  await settle(40);
  const ch = p.document.querySelector('[data-key="ANALOG_1"]');
  const inputs = ch.querySelectorAll('.inverted-section input');
  const [, rmin, rmax, cal] = inputs;
  assert.ok(!rmin.disabled);
  check(cal, true);
  assert.ok(rmin.disabled && rmax.disabled);
  // Einseitiger Kanal: untere Schwellen ausgeblendet
  const pm = p.document.querySelector('[data-key="SDS011_0"]');
  const rows = pm.querySelectorAll('.threshold-row');
  assert.equal(rows[0].style.display, 'none');
  assert.equal(rows[1].style.display, 'none');
  assert.notEqual(rows[2].style.display, 'none');
  assert.ok(pm.querySelector('.threshold-container').classList.contains('one-sided'));
});

test('Sensoren: Halbwertszeit nur mit Autokalibrierung, Regler folgt den Werten', async () => {
  const p = open('sensors.html');
  await settle(40);
  const ch = p.document.querySelector('[data-key="ANALOG_1"]');
  const half = ch.querySelector('select.half-life');
  const cal = ch.querySelectorAll('.inverted-section input')[3];
  const halfRow = half.parentNode.parentNode;
  assert.equal(halfRow.style.display, cal.checked ? '' : 'none');
  check(cal, !cal.checked);
  assert.equal(halfRow.style.display, cal.checked ? '' : 'none');
  assert.ok(ch.querySelector('.ts-chip-value').textContent.length > 0, 'aktueller Wert am Regler');
  assert.equal(ch.querySelectorAll('.ts-top .ts-chip').length, 4);
});

test('Schwellenregler: Beschriftungen, Live-Werte, Pfeiltasten', async () => {
  const p = open('sensors.html');
  await settle(40);
  const S = p.ctx.ThresholdSlider;
  const ch = { unit: '%', yellowLow: 10, greenLow: 20, greenHigh: 80, yellowHigh: 90, value: 50, obsMin: 40, obsMax: 60 };
  const changes = [];
  const sl = S.create(ch, (l, k) => changes.push([k, l[k]]));
  const box = sl.element;
  const chips = box.querySelectorAll('.ts-top .ts-chip').map((c) => c.textContent);
  assert.deepEqual(chips, ['10', '20', '80', '90']);
  assert.equal(box.querySelectorAll('.ts-tick').length, 6);
  assert.equal(box.querySelector('.ts-chip-value').textContent, '50.0%');
  sl.update({ value: null, obsMin: null, obsMax: null });
  assert.equal(box.querySelector('.measured-value-dot').style.display, 'none');
  assert.equal(box.querySelector('.ts-chip-value').style.display, 'none');
  sl.update({ value: 12.5, obsMin: 5, obsMax: 30 });
  assert.equal(box.querySelector('.ts-chip-value').textContent, '12.5%');
  assert.equal(box.querySelector('.ts-observed').style.display, '');
  // Pfeiltaste verschiebt einen Griff und meldet die Änderung
  const h = box.querySelector('[data-key="greenLow"]');
  h.dispatch('keydown', { key: 'ArrowRight', preventDefault() {} });
  assert.deepEqual(changes.at(-1), ['greenLow', 21]);
  // einseitig: nur zwei Beschriftungen
  const one = S.create({ unit: 'µg/m³', oneSided: true, yellowLow: 0, greenLow: 0, greenHigh: 25, yellowHigh: 50, value: 3 }, () => {});
  assert.equal(one.element.querySelectorAll('.ts-top .ts-chip').length, 2);
});

test('Sensoren: Sensor deaktivieren, Intervall, Messen, Extrema zurücksetzen', async () => {
  const p = open('sensors.html');
  await settle(40);
  const card = p.document.querySelector('[data-sensor="DHT"]');
  const [enabled] = card.querySelectorAll('input[type="checkbox"]');
  check(enabled, false);
  p.clock.advance(10);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH')[0]), { DHT: { enabled: false } });
  assert.ok(card.classList.contains('disabled'));
  const interval = card.querySelector('input[type="number"]');
  type(interval, '120');
  p.clock.advance(1100);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH')[1]), { DHT: { interval: 120 } });
  card.querySelectorAll('button').find((b) => b.textContent === 'Messen').dispatch('click');
  card.querySelectorAll('button').find((b) => b.textContent === 'Zurücksetzen').dispatch('click');
  await settle();
  const posts = writes(p, 'POST');
  assert.equal(posts[0].url, '/api/v1/sensors/DHT/measure');
  assert.equal(posts[1].url, '/api/v1/sensors/DHT/0/reset-extrema');
  assert.deepEqual(body(posts[1]), { raw: false });
});

test('Sensoren: Blumengesicht und LED-Ampel aus der Konfiguration', async () => {
  const p = open('sensors.html');
  await settle(40);
  const d = p.document;
  assert.equal(d.getElementById('flower').value, 'ANALOG_1');
  assert.equal(d.getElementById('flower').children.length, fixture('config.json').channels.length);
  assert.equal(d.getElementById('ledMode').value, '2');
  assert.equal(d.getElementById('ledChannelGroup').style.display, '');
  type(d.getElementById('ledMode'), '0');
  assert.equal(d.getElementById('ledChannelGroup').style.display, 'none');
  p.clock.advance(1100);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH')[0]), { led: { mode: '0' } });
});

test('Sensoren: Fehlerantwort des Geräts markiert das Feld', async () => {
  const p = open('sensors.html', { 'PATCH /api/v1/sensors': { status: 400, body: fixture('error_patch.json') } });
  await settle(40);
  const card = p.document.querySelector('[data-sensor="DHT"]');
  const interval = card.querySelector('input[type="number"]');
  type(interval, '5');
  p.clock.advance(1100);
  await settle();
  assert.ok(interval.classList.contains('invalid'));
  assert.ok(p.document.querySelectorAll('.toast-error').length >= 1);
});

// ---------------------------------------------------------------------------
test('Display: Schalter und Kanäle für das Display', async () => {
  const p = open('display.html');
  await settle(40);
  const d = p.document;
  const dur = d.querySelector('[data-path="display.screenDuration"]');
  assert.equal(dur.value, 5);
  const list = d.getElementById('displayChannels').querySelectorAll('input');
  assert.equal(list.length, fixture('sensors.json').sensors.reduce((n, s) => n + s.channels.length, 0));
  check(list[1], false);
  p.clock.advance(400);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH')[0]), { ANALOG: { channels: { 1: { display: false } } } });
});

// ---------------------------------------------------------------------------
test('Logs: Zeilen aus dem Ring, Level-Knopf, Fehler und Wiederholung', async () => {
  let fail = false;
  const p = open('logs.html', { '/api/v1/logs': () => (fail ? new Error('weg') : { body: { next: 5, dropped: 0, lines: [{ seq: 4, ts: '1s', level: 'WARNING', module: 'WLAN', text: 'Hallo' }] } }) });
  await settle(40);
  const d = p.document;
  const box = d.getElementById('logBox');
  assert.equal(box.children.length, 1);
  assert.ok(box.children[0].classList.contains('warning'));
  assert.match(box.children[0].textContent, /\[WLAN\] Hallo/);
  assert.equal(d.getElementById('logStatus').textContent, 'verbunden');
  // nächster Poll fragt ab der gelieferten Sequenz
  p.clock.advance(1000);
  await settle();
  assert.ok(p.requests.some((r) => r.url === '/api/v1/logs?since=5'));
  // Level setzen
  d.querySelector('[data-level="ERROR"]').dispatch('click');
  await settle();
  assert.equal(writes(p, 'PATCH')[0].url, '/api/v1/logs/settings');
  assert.deepEqual(body(writes(p, 'PATCH')[0]), { log: { level: 'ERROR' } });
  assert.ok(!p.requests.some((r) => r.url.startsWith('/api/v1/config')), 'keine Admin-API auf der Logs-Seite');
  // Debug-Schalter nur bei Level DEBUG bedienbar, speichern sofort
  const ram = d.querySelector('[data-log="debugRam"]');
  assert.ok(ram.disabled, 'bei ERROR gesperrt');
  d.querySelector('[data-level="DEBUG"]').dispatch('click');
  await settle();
  assert.ok(!ram.disabled, 'bei DEBUG bedienbar');
  ram.checked = true; ram.dispatch('change');
  p.clock.advance(10);
  await settle();
  assert.deepEqual(body(writes(p, 'PATCH')[2]), { log: { debugRam: true } });
  assert.ok(d.querySelector('[data-level="DEBUG"]').classList.contains('active'));
  assert.ok(!d.querySelector('[data-level="ERROR"]').classList.contains('active'));
  // Verbindung weg: Wiederholung mit wachsendem Abstand, nach 5 aufgegeben
  fail = true;
  for (let i = 1; i <= 6; i++) { p.clock.advance(3000 * i + 1000); await settle(); }
  assert.match(d.getElementById('logStatus').textContent, /neu laden/);
  // Leeren
  d.getElementById('logClear').dispatch('click');
  assert.equal(box.children.length, 0);
});

test('Logs: höchstens 100 Einträge, verpasste Zeilen werden angezeigt', async () => {
  let seq = 0;
  const p = open('logs.html', { '/api/v1/logs': () => {
    const lines = Array.from({ length: 30 }, () => ({ seq: seq++, ts: '1s', level: 'INFO', module: 'M', text: 't' }));
    return { body: { next: seq, dropped: seq > 30 ? 3 : 0, lines } };
  } });
  for (let i = 0; i < 6; i++) { p.clock.advance(1000); await settle(); }
  const box = p.document.getElementById('logBox');
  assert.equal(box.children.length, 100);
  assert.ok(box.querySelectorAll('.system').length >= 1);
});

// ---------------------------------------------------------------------------
test('E-Mail: Formular aus der Konfiguration, Speichern mit Schlüssel, Testmail mit Status', async () => {
  let pending = true;
  const p = open('mail.html', { '/api/v1/mail/status': () => ({ body: { pending, sent: 3, result: 'Versand erfolgreich', lastAttempt: 1762257600 } }) });
  await settle(40);
  const d = p.document;
  assert.ok(d.getElementById('mail-enabled').checked);
  assert.equal(d.getElementById('mail-to').value, 'ich@example.org');
  assert.equal(d.getElementById('mail-warnFrom').value, 'yellow');
  const watch = d.getElementById('mail-watch').querySelectorAll('input');
  assert.equal(watch.filter((c) => c.checked).length, 1, 'nur ANALOG_1 überwacht');
  assert.match(d.getElementById('mail-status').textContent, /3 erfolgreich/);
  d.getElementById('mail-key').value = 'AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=';
  d.getElementById('mail-save-test').dispatch('click');
  await settle(40);
  const patch = body(writes(p, 'PATCH')[0]);
  assert.equal(patch.mail.to, 'ich@example.org');
  assert.equal(patch.mail.watch, 'ANALOG_1');
  assert.equal(patch.mail.warnHours, 4);
  assert.equal(writes(p, 'PUT')[0].url, '/api/v1/secrets/mail-key');
  assert.equal(writes(p, 'POST')[0].url, '/api/v1/mail/test');
  assert.equal(d.getElementById('mail-key').value, '');
  // Alle angehakt = leere Auswahl (alle)
  watch.forEach((c) => { c.checked = true; });
  assert.equal(p.ctx.Mail.readForm().watch, '');
  pending = false;
  p.clock.advance(3000);
  await settle();
  // Ausschalten graut die Felder aus
  check(d.getElementById('mail-enabled'), false);
  assert.ok(d.getElementById('mail-fields').classList.contains('aus'));
});

// ---------------------------------------------------------------------------
test('Mailtexte: Tabs, Zähler, Prüfhinweise, Vorschau, Speichern und Zurücksetzen', async () => {
  const p = open('mail-templates.html');
  await settle(40);
  const d = p.document;
  const tabs = d.getElementById('tplTabs').querySelectorAll('button');
  assert.equal(tabs.length, 4);
  assert.ok(tabs[0].classList.contains('active'));
  const body0 = d.getElementById('tplBody');
  assert.match(body0.value, /\{messwerte\}/);
  assert.match(d.getElementById('tplCounter').textContent, /\/ 1600 Bytes/);
  assert.match(d.getElementById('tplPreview').srcdoc, /<table class="werte">/);
  assert.match(d.getElementById('tplSubjectPreview').textContent, /^\[Frameclaw PS\] /);
  // Unbekannter Platzhalter → Hinweis
  type(body0, 'Hallo {quatsch}');
  assert.match(d.getElementById('tplChecks').textContent, /\{quatsch\} kennt das Gerät nicht/);
  // Emoji einfügen
  body0.selectionStart = body0.selectionEnd = 0;
  d.getElementById('tplEmojis').querySelectorAll('button')[0].dispatch('click');
  assert.ok(body0.value.startsWith('🌱'));
  // Zu lang → roter Zähler
  type(body0, 'x'.repeat(1700));
  assert.equal(d.getElementById('tplCounter').className, 'counter over');
  type(body0, 'Kurz');
  d.getElementById('tplSave').dispatch('click');
  await settle(20);
  const put = writes(p, 'PUT')[0];
  assert.equal(put.url, '/api/v1/mail/templates/boot');
  assert.equal(body(put).body, 'Kurz');
  d.getElementById('tplReset').dispatch('click');
  await settle(20);
  assert.equal(writes(p, 'DELETE')[0].url, '/api/v1/mail/templates/boot');
  // Design-Tab: Betreff ausgeblendet, <> verboten
  tabs[3].dispatch('click');
  await settle(20);
  assert.equal(d.getElementById('subjectGroup').style.display, 'none');
  type(body0, 'p{}<script>');
  assert.match(d.getElementById('tplChecks').textContent, /Spitze Klammern/);
});

// ---------------------------------------------------------------------------
test('Update: Version, Pflicht-MD5, Upload mit echtem Fortschritt', async () => {
  const xhrs = [];
  class FakeXhr {
    constructor() { this.upload = {}; xhrs.push(this); }
    open(m, u) { this.method = m; this.url = u; }
    send(form) { this.form = form; }
  }
  const cfg = fixture('config.json');
  cfg.config.general.otaRequireMd5 = true;
  const p = open('update.html', { '/api/v1/config': { body: cfg }, '/status': { body: { uptime: 50, bootId: 9 } } },
                 { extra: { XMLHttpRequest: FakeXhr } });
  await settle(40);
  const d = p.document;
  assert.match(d.getElementById('otaVersion').textContent, /^3\.0\.0/);
  assert.match(d.getElementById('md5Group').querySelector('label').textContent, /verlangt/);
  d.getElementById('otaStart').dispatch('click');
  assert.ok(toasts(p).some((t) => /firmware\.bin/.test(t)));
  d.getElementById('otaFile').files = [{ name: 'firmware.bin', size: 400000 }];
  d.getElementById('otaStart').dispatch('click');
  assert.ok(toasts(p).some((t) => /verlangt/.test(t)));
  d.getElementById('otaMd5').value = 'zz';
  d.getElementById('otaStart').dispatch('click');
  assert.ok(toasts(p).some((t) => /32 Hex/.test(t)));
  d.getElementById('otaMd5').value = '0123456789abcdef0123456789abcdef';
  d.getElementById('otaStart').dispatch('click');
  await settle(20);
  assert.equal(xhrs.length, 1);
  assert.equal(xhrs[0].url, '/api/v1/ota?size=400000&md5=0123456789abcdef0123456789abcdef');
  xhrs[0].upload.onprogress({ lengthComputable: true, loaded: 200000, total: 400000 });
  assert.equal(d.getElementById('otaBar').style.width, '50%');
  xhrs[0].status = 400;
  xhrs[0].responseText = JSON.stringify({ ok: false, message: 'Keine gültige Firmware-Datei' });
  xhrs[0].onload();
  assert.match(d.getElementById('otaStatus').textContent, /Keine gültige Firmware.*alte Firmware bleibt/);
  xhrs[0].onerror();
  assert.match(d.getElementById('otaStatus').textContent, /abgebrochen/);
});

// ---------------------------------------------------------------------------
test('Anmeldung: nur Passwort, danach zurück zur Seite; Fehler werden gemeldet', async () => {
  let ok = false;
  const p = open('login.html', { 'POST /api/v1/login': () => (ok ? { body: { ok: true } } : { status: 401, body: { ok: false, error: 'auth', message: 'Passwort falsch' } }) });
  await settle(20);
  const d = p.document;
  assert.equal(d.querySelectorAll('input').length, 1, 'nur ein Feld');
  d.getElementById('loginPassword').value = 'falsch';
  d.getElementById('loginForm').dispatch('submit', { preventDefault() {} });
  await settle();
  assert.equal(body(writes(p, 'POST')[0]).password, 'falsch');
  assert.match(d.getElementById('loginMsg').textContent, /falsch/);
  const L = p.ctx.Login;
  assert.equal(L.nextTarget('?next=%2Fadmin%2Fsensors'), '/admin/sensors');
  assert.equal(L.nextTarget(''), '/admin');
  assert.equal(L.nextTarget('?next=//evil.example'), '/admin', 'keine fremden Ziele');
  assert.equal(L.nextTarget('?next=https://evil.example'), '/admin');
  ok = true;
});
