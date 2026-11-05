/** Startseite (start.js + footer.js) im Mini-DOM */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadPage, settle } from './dom.mjs';
import { makeClock } from './load.mjs';

const SPEC = [{ cls: ['box', 'status-unknown'], children: [
  { id: 'cloud', cls: ['cloud'] },
  { tag: 'img', cls: ['face'], attrs: { src: '/img/face-neutral.gif' } },
  { id: 'sensors', cls: ['sensors-container'] },
  { id: 'footer' }
] }];

const SYSTEM = {
  deviceName: 'Balkon', version: '3.0.0', buildDate: 'Nov  4 2026', ip: '1.2.3.4', ssid: 'Netz',
  hostname: 'balkon', ap: false, rssi: -61, uptime: 3700, rebootCount: 7, time: '04.11.2026 12:00',
  features: { display: true, led: true },
  channels: [
    { key: 'ANALOG_0', sensor: 'ANALOG', name: 'Licht', unit: '%' },
    { key: 'ANALOG_1', sensor: 'ANALOG', name: 'Boden', unit: '%' },
    { key: 'DHT_0', sensor: 'DHT', name: 'Luft', unit: '°C' }
  ]
};

function values(last = 10000, extra = {}) {
  return {
    currentTime: 20000, flowerStatusSensor: 'ANALOG_1',
    sensors: {
      ANALOG_0: { value: 42.17, unit: '%', status: 'yellow', lastMeasurement: last, measurementInterval: 60000 },
      ANALOG_1: { value: 55, unit: '%', status: 'green', lastMeasurement: last, measurementInterval: 60000 },
      DHT_0: { value: null, unit: '°C', status: 'error', lastMeasurement: last, measurementInterval: 60000 },
      ...extra
    }
  };
}

function page({ measure = { status: 200, body: { ok: true } }, latest = () => values(), clock = makeClock(100000) } = {}) {
  const p = loadPage(['ui.js', 'api.js', 'footer.js', 'start.js'], {
    spec: SPEC, clock,
    respond: (url) => {
      if (url === '/api/v1/system') return { body: SYSTEM };
      if (url === '/getLatestValues') return { body: latest() };
      if (url === '/api/v1/measure') return typeof measure === 'function' ? measure() : measure;
      return { status: 404, body: {} };
    }
  });
  return p;
}

const rows = (d) => d.querySelectorAll('.sensor');
const row = (d, key) => d.querySelector(`[data-sensor="${key}"]`);
const txt = (r, sel) => r.querySelector(sel).textContent;

test('Karten aus der Kanalliste, abwechselnd links und rechts, Werte und Status', async () => {
  const p = page();
  p.start();
  await settle();
  const d = p.document;
  assert.equal(rows(d).length, 3);
  assert.ok(rows(d)[0].classList.contains('left'));
  assert.ok(rows(d)[1].classList.contains('right'));
  assert.equal(txt(row(d, 'ANALOG_0'), '.label span'), 'LICHT');
  assert.equal(txt(row(d, 'ANALOG_0'), '.value span'), '42.2%');
  assert.equal(txt(row(d, 'ANALOG_0'), '.status span'), 'STATUS: Warnung');
  assert.equal(txt(row(d, 'DHT_0'), '.value span'), '--');
  assert.equal(txt(row(d, 'DHT_0'), '.status span'), 'STATUS: Fehler');
  assert.ok(row(d, 'ANALOG_0').classList.contains('sensor-status-yellow'));
  // Blume folgt ANALOG_1 (grün), die Karte ist aktiv
  assert.equal(d.querySelector('.face').src, '/img/face-happy.gif');
  assert.ok(d.querySelector('.box').classList.contains('status-green'));
  assert.ok(row(d, 'ANALOG_1').classList.contains('active'));
  assert.ok(!row(d, 'ANALOG_0').classList.contains('active'));
  // Countdown aus Serverzeit: 20000 - 10000 = 10 s
  assert.equal(txt(row(d, 'ANALOG_0'), '.interval span'), '(10s/60s)');
  // Wolke und Titel aus /api/v1/system
  assert.equal(d.getElementById('cloud-label').textContent, 'Balkon');
  assert.equal(d.title, 'Balkon');
});

test('Countdown zählt lokal sekündlich hoch', async () => {
  const p = page();
  p.start();
  await settle();
  p.clock.advance(5000);
  p.ctx.Start.tickCountdowns();
  assert.equal(txt(row(p.document, 'ANALOG_0'), '.interval span'), '(15s/60s)');
});

test('Klick misst alle Karten des Sensors, Schnell-Polls bis zum neuen Wert, dann Wackeln', async () => {
  let last = 10000;
  const p = page({ latest: () => values(last) });
  p.start();
  await settle();
  const d = p.document;
  row(d, 'ANALOG_0').dispatch('click');
  await settle();
  const post = p.requests.find((r) => r.url === '/api/v1/measure');
  assert.equal(post.method, 'POST');
  assert.deepEqual(JSON.parse(post.body), { sensor: 'ANALOG' });
  assert.ok(row(d, 'ANALOG_0').classList.contains('measuring'));
  assert.ok(row(d, 'ANALOG_1').classList.contains('measuring'));
  assert.ok(!row(d, 'DHT_0').classList.contains('measuring'));
  assert.equal(txt(row(d, 'ANALOG_1'), '.interval span'), '(misst …)');
  // Erster Schnell-Poll ohne neue Messung
  p.clock.advance(1500);
  await settle();
  assert.ok(row(d, 'ANALOG_0').classList.contains('measuring'));
  // Neuer Messzeitpunkt → wackeln
  last = 20500;
  p.clock.advance(2000);
  await settle();
  assert.ok(!row(d, 'ANALOG_0').classList.contains('measuring'));
  assert.ok(row(d, 'ANALOG_0').classList.contains('wiggle'));
  p.clock.advance(800);
  assert.ok(!row(d, 'ANALOG_0').classList.contains('wiggle'));
  // Nach Ablauf der Sperre (3 s) darf wieder gemessen werden
  row(d, 'ANALOG_0').dispatch('click');
  await settle();
  assert.equal(p.requests.filter((r) => r.url === '/api/v1/measure').length, 2);
});

test('während der Messung und in der 3-s-Sperre wird nichts gesendet', async () => {
  const p = page();
  p.start();
  await settle();
  const r = row(p.document, 'ANALOG_0');
  r.dispatch('click');
  await settle();
  r.dispatch('click');  // misst noch
  row(p.document, 'ANALOG_1').dispatch('click');  // derselbe Sensor
  await settle();
  assert.equal(p.requests.filter((x) => x.url === '/api/v1/measure').length, 1);
  // Messung vorbei, aber Sperre aktiv
  r.classList.remove('measuring');
  p.clock.advance(1000);
  r.dispatch('click');
  await settle();
  assert.equal(p.requests.filter((x) => x.url === '/api/v1/measure').length, 1);
});

test('Enter und Leertaste lösen aus, andere Tasten nicht', async () => {
  const p = page();
  p.start();
  await settle();
  const r = row(p.document, 'DHT_0');
  const ev = r.dispatch('keydown', { key: 'a' });
  assert.equal(ev.defaultPrevented, false);
  r.dispatch('keydown', { key: 'Enter' });
  await settle();
  assert.equal(p.requests.filter((x) => x.url === '/api/v1/measure').length, 1);
});

test('429 zeigt "(bitte warten)", Fehler "(Fehler)", je 3 s lang', async () => {
  const p = page({ measure: { status: 429, body: {} } });
  p.start();
  await settle();
  const r = row(p.document, 'DHT_0');
  r.dispatch('click');
  await settle();
  assert.equal(txt(r, '.interval span'), '(bitte warten)');
  assert.ok(!r.classList.contains('measuring'));
  p.ctx.Start.poll();
  await settle();
  assert.equal(txt(r, '.interval span'), '(bitte warten)');  // Notiz unterdrückt den Countdown
  p.clock.advance(3100);
  p.ctx.Start.tickCountdowns();
  assert.notEqual(txt(r, '.interval span'), '(bitte warten)');
  const q = page({ measure: () => new Error('weg') });
  q.start();
  await settle();
  const r2 = row(q.document, 'DHT_0');
  r2.dispatch('click');
  await settle();
  assert.equal(txt(r2, '.interval span'), '(Fehler)');
});

test('Notbremse nach 25 s: "(keine Antwort)"', async () => {
  const p = page();
  p.start();
  await settle();
  const r = row(p.document, 'DHT_0');
  r.dispatch('click');
  await settle();
  p.clock.advance(26000);
  await settle();
  p.ctx.Start.tickCountdowns();
  assert.ok(!r.classList.contains('measuring'));
  assert.equal(txt(r, '.interval span'), '(keine Antwort)');
});

test('nie zwei Polls parallel; nach drei Fehlern Fehlergesicht', async () => {
  let fail = false;
  const p = page({ latest: () => { if (fail) throw new Error('weg'); return values(); } });
  p.start();
  await settle();
  const before = p.requests.filter((r) => r.url === '/getLatestValues').length;
  p.ctx.Start.poll();
  p.ctx.Start.poll();
  await settle();
  assert.equal(p.requests.filter((r) => r.url === '/getLatestValues').length, before + 1);
  fail = true;
  for (let i = 0; i < 3; i++) { p.ctx.Start.poll(); await settle(); }
  assert.equal(p.document.querySelector('.face').src, '/img/face-error.gif');
});

test('Fußzeile: Navigation, Status, Admin-Menü klappt auf der Startseite auf', async () => {
  const p = page();
  p.start();
  await settle();
  const d = p.document;
  assert.ok(d.getElementById('nav-start').classList.contains('active'));
  assert.equal(d.getElementById('f-name').textContent, 'balkon.local');
  assert.equal(d.getElementById('f-rssi').textContent, '-61 dBm');
  assert.equal(d.getElementById('f-uptime').textContent, '1h 1m');
  assert.equal(d.getElementById('f-reboots').textContent, '7');
  assert.equal(d.getElementById('f-version').textContent, 'V 3.0.0');
  const menu = d.getElementById('footer-admin-menu');
  assert.equal(menu.style.display, 'none');
  const ev = d.getElementById('nav-admin').dispatch('click');
  assert.ok(ev.defaultPrevented);
  assert.equal(menu.style.display, '');
  assert.equal(d.getElementById('footer-stats-table').style.display, 'none');
  d.getElementById('nav-admin').dispatch('click');
  assert.equal(menu.style.display, 'none');
});

test('Fußzeile ohne Display blendet den Display-Eintrag aus; AP-Modus', async () => {
  const p = loadPage(['ui.js', 'api.js', 'footer.js'], {
    spec: [{ id: 'cloud' }, { id: 'footer' }],
    location: { pathname: '/admin/sensors' },
    respond: () => ({ body: { ...SYSTEM, ap: true, ssid: 'Balkon', features: { display: false } } })
  });
  p.document.body.dataset.title = 'Sensoren';
  p.start();
  await settle();
  const d = p.document;
  assert.ok(d.getElementById('nav-admin').classList.contains('active'));
  assert.equal(d.getElementById('footer-admin-menu').style.display, '');
  assert.equal(d.querySelector('[data-feature="display"]').style.display, 'none');
  assert.equal(d.getElementById('f-ssid').textContent, 'Balkon (AP)');
  assert.equal(d.getElementById('f-rssi').textContent, '–');
  assert.equal(d.title, 'Sensoren – Balkon');
  const active = d.querySelectorAll('#footer-admin-menu .nav-item.active');
  assert.equal(active.length, 1);
});

test('Text des Blatts sitzt in .card-inner und wackelt mit', async () => {
  const p = page();
  p.start();
  await settle();
  const r = row(p.document, 'ANALOG_0');
  const inner = r.querySelector('.card .card-inner');
  assert.ok(inner, '.card-inner fehlt');
  for (const sel of ['.label', '.value', '.status', '.interval']) assert.ok(inner.querySelector(sel), sel);
});

test('Drehpunkt des Texts: Blattmitte im Koordinatensystem der gedrehten Karte', () => {
  const { Start } = page().ctx;
  assert.deepEqual([...Start.parseMatrix('none')], [1, 0, 0, 1, 0, 0]);
  assert.deepEqual([...Start.parseMatrix('matrix(0.5, 0.25, -0.25, 0.5, 3, -4)')], [0.5, 0.25, -0.25, 0.5, 3, -4]);
  const m3 = 'matrix3d(1, 2, 0, 0, 3, 4, 0, 0, 0, 0, 1, 0, 5, 6, 0, 1)';
  assert.deepEqual([...Start.parseMatrix(m3)], [1, 2, 3, 4, 5, 6]);
  const close = (p, x, y) => {
    assert.ok(Math.abs(p.x - x) < 1e-9 && Math.abs(p.y - y) < 1e-9, `(${p.x}, ${p.y}) statt (${x}, ${y})`);
  };
  // Ungedreht: Punkt bleibt, wo er ist
  close(Start.pivotInCard(120, 30, 50, 20, [1, 0, 0, 1, 0, 0]), 120, 30);
  // translateY(-50%) bei 40 px Höhe: im Inneren liegt der Punkt 20 px tiefer
  close(Start.pivotInCard(120, 30, 50, 20, [1, 0, 0, 1, 0, -20]), 120, 50);
  // 180° um den Ursprung (Fehlerzustand): gespiegelt um O
  close(Start.pivotInCard(120, 30, 50, 20, [-1, 0, 0, -1, 0, 0]), -20, 10);
  // Hin und zurück mit beliebiger Drehung + Verschiebung
  const a = 16 * Math.PI / 180, m = [Math.cos(a), Math.sin(a), -Math.sin(a), Math.cos(a), 0, -17];
  const local = Start.pivotInCard(150, 40, 60, 17, m);
  const screenX = 60 + m[0] * (local.x - 60) + m[2] * (local.y - 17) + m[4];
  const screenY = 17 + m[1] * (local.x - 60) + m[3] * (local.y - 17) + m[5];
  close({ x: screenX, y: screenY }, 150, 40);
});
