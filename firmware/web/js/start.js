/**
 * @file start.js
 * @brief Startseite: Blume, Blätter, Live-Werte, Sofortmessung
 *
 * Übernommen aus der Firmware 2.x (data/js/sensors.js), jetzt mit Karten aus
 * /api/v1/system (Kanalliste) und Werten aus /getLatestValues.
 *  - Poll alle 10 s, nur bei sichtbarem Tab, nie parallel; nach drei Fehlern Fehlerzustand
 *  - Klick/Enter/Leertaste auf ein Blatt: Sofortmessung des Sensors
 *    (POST /api/v1/measure), danach Schnell-Polls, Wackeln bei neuem Wert
 *  - Der Text auf dem Blatt (.card-inner) wackelt und atmet mit: gleiche
 *    Animation, Drehpunkt per alignCardPivot() auf die Blattmitte gelegt
 */
(function (global) {
  'use strict';

  const POLL_MS = 10000;
  const MAX_FAILURES = 3;
  const MEASURE_COOLDOWN_MS = 3000;
  const MEASURING_TIMEOUT_MS = 25000;
  const WIGGLE_MS = 700;
  const FAST_POLL_DELAYS_MS = [1500, 2000, 2500, 3000, 4000, 5000];
  const STATUS_CLASSES = ['green', 'yellow', 'red', 'error', 'warmup', 'unknown'];
  const FACES = { green: 'face-happy.gif', yellow: 'face-neutral.gif', red: 'face-sad.gif' };

  const state = {
    failures: 0,
    inFlight: false,
    serverOffset: null,
    cooldown: {},
    fastTimer: null,
    fastStep: 0,
    channels: []
  };

  // === Reine Funktionen (getestet) ===

  function faceFor(status) {
    return '/img/' + (FACES[status] || 'face-error.gif');
  }

  /** 2D-Matrix aus getComputedStyle(...).transform: [a, b, c, d, e, f] */
  function parseMatrix(text) {
    const m = /^matrix(3d)?\((.*)\)$/.exec(String(text || '').trim());
    if (!m) return [1, 0, 0, 1, 0, 0];
    const v = m[2].split(',').map(Number);
    return m[1] ? [v[0], v[1], v[4], v[5], v[12], v[13]] : v.slice(0, 6);
  }

  /**
   * Punkt (px, py) aus den Layout-Koordinaten der Karte in ihr eigenes,
   * transformiertes Koordinatensystem umrechnen. Die Karte bildet x auf
   * O + M·(x − O) ab (O = transform-origin); gesucht ist x = O + M⁻¹·(p − O).
   * Dreht man den Inhalt um diesen Punkt, dreht er sich auf dem Bildschirm um p.
   */
  function pivotInCard(px, py, ox, oy, m) {
    const [a, b, c, d, e, f] = m;
    const det = a * d - b * c;
    if (!det) return { x: px, y: py };
    const X = px - ox - e, Y = py - oy - f;
    return { x: ox + (d * X - c * Y) / det, y: oy + (-b * X + a * Y) / det };
  }

  function intervalText(lastMeasurement, intervalMs, serverNow) {
    if (!lastMeasurement || !intervalMs) return '(--/--)';
    const elapsed = Math.max(0, Math.floor((serverNow - lastMeasurement) / 1000));
    return `(${elapsed}s/${Math.floor(intervalMs / 1000)}s)`;
  }

  // === DOM ===

  const doc = () => global.document;

  function card(ch, index) {
    const { el } = global.UI;
    const node = el('div', {
      class: 'sensor ' + (index % 2 === 0 ? 'left' : 'right') + ' sensor-status-unknown',
      'data-sensor': ch.key, 'data-sensor-id': ch.sensor, role: 'button', tabindex: '0',
      title: 'Klicken für eine Sofortmessung'
    }, [
      el('img', { class: 'stem', src: '/img/sensor-stem.png', alt: '' }),
      el('span', { class: 'leaf-wrap' }, [el('img', { class: 'leaf', src: '/img/sensor-leaf.png', alt: '' })]),
      el('div', { class: 'card' }, [el('div', { class: 'card-inner' }, [
        el('div', { class: 'label' }, [el('span', { text: ch.name.toUpperCase() })]),
        el('div', { class: 'value' }, [el('span', { text: '--' })]),
        el('div', { class: 'status unknown' }, [el('span', { text: 'STATUS: ' + global.UI.statusText('unknown') })]),
        el('div', { class: 'interval' }, [el('span', { text: '(--/--)' })])
      ])])
    ]);
    const go = () => triggerMeasurement(node);
    node.addEventListener('click', go);
    node.addEventListener('keydown', e => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        go();
      }
    });
    return node;
  }

  function renderCards(channels) {
    const box = doc().getElementById('sensors');
    if (!box) return;
    const keys = channels.map(c => c.key).join(',');
    if (box.dataset.keys === keys) return;
    box.dataset.keys = keys;
    box.textContent = '';
    channels.forEach((c, i) => box.appendChild(card(c, i)));
  }

  function serverNow() {
    return state.serverOffset === null ? Date.now() : Date.now() - state.serverOffset;
  }

  function rows() { return Array.from(doc().querySelectorAll('.sensor')); }
  function rowsOf(sensorId) { return rows().filter(r => r.dataset.sensorId === sensorId); }

  function noteActive(row) {
    const until = parseInt(row.dataset.noteUntil, 10);
    if (!until) return false;
    if (Date.now() < until) return true;
    delete row.dataset.noteUntil;
    return false;
  }

  function suppressed(row) { return row.classList.contains('measuring') || noteActive(row); }

  function setNote(row, text, ms) {
    row.dataset.noteUntil = String(Date.now() + ms);
    const s = row.querySelector('.interval span');
    if (s) s.textContent = text;
  }

  /**
   * Drehpunkt des Texts auf die Mitte des Blatts legen, damit er beim Wackeln
   * mitschwingt statt auf der Stelle zu zittern. Die Karte ist je nach Status
   * selbst gedreht (gelb/rot, Fehler 180°) – deshalb über ihre Matrix. Ohne
   * Layout-Angaben (Tests, alte Browser) bleibt es bei der Mitte der Karte.
   */
  function alignCardPivot(row) {
    const card = row.querySelector('.card'), inner = row.querySelector('.card-inner');
    const leaf = row.querySelector('.leaf-wrap');
    if (!card || !inner || !leaf || typeof global.getComputedStyle !== 'function') return;
    const cs = global.getComputedStyle(card);
    const origin = String(cs.transformOrigin || '').split(' ').map(parseFloat);
    if (origin.length < 2 || origin.some(isNaN)) return;
    const p = pivotInCard(leaf.offsetLeft + leaf.offsetWidth / 2 - card.offsetLeft,
      leaf.offsetTop + leaf.offsetHeight / 2 - card.offsetTop, origin[0], origin[1], parseMatrix(cs.transform));
    inner.style.transformOrigin = (p.x - inner.offsetLeft) + 'px ' + (p.y - inner.offsetTop) + 'px';
  }

  function startMeasuring(row) {
    alignCardPivot(row);
    row.classList.add('measuring');
    row.dataset.measuringSince = String(Date.now());
    row.setAttribute('aria-busy', 'true');
    const s = row.querySelector('.interval span');
    if (s) s.textContent = '(misst …)';
  }

  function stopMeasuring(row) {
    row.classList.remove('measuring');
    delete row.dataset.measuringSince;
    row.removeAttribute('aria-busy');
  }

  function releaseStale() {
    doc().querySelectorAll('.sensor.measuring').forEach(row => {
      const since = parseInt(row.dataset.measuringSince, 10);
      if (since && Date.now() - since <= MEASURING_TIMEOUT_MS) return;
      stopMeasuring(row);
      setNote(row, '(keine Antwort)', 3000);
    });
  }

  function wiggle(row) {
    stopMeasuring(row);
    delete row.dataset.noteUntil;
    row.classList.remove('wiggle');
    alignCardPivot(row);
    void row.offsetWidth;
    row.classList.add('wiggle');
    setTimeout(() => row.classList.remove('wiggle'), WIGGLE_MS);
  }

  function updateRow(row, d) {
    row.querySelector('.value span').textContent = global.UI.formatValue(d.value, d.unit);
    const st = STATUS_CLASSES.includes(d.status) ? d.status : 'unknown';
    const status = row.querySelector('.status');
    status.classList.remove(...STATUS_CLASSES);
    status.classList.add(st);
    status.querySelector('span').textContent = 'STATUS: ' + global.UI.statusText(st);
    row.classList.remove(...STATUS_CLASSES.map(s => 'sensor-status-' + s));
    row.classList.add('sensor-status-' + st);
    const prev = row.dataset.lastMeasurement;
    const cur = String(d.lastMeasurement);
    row.dataset.lastMeasurement = cur;
    row.dataset.measurementInterval = String(d.measurementInterval);
    if (prev && prev !== cur) wiggle(row);
    if (!suppressed(row)) {
      row.querySelector('.interval span').textContent =
        intervalText(d.lastMeasurement, d.measurementInterval, serverNow());
    }
  }

  function updateFlower(status) {
    const face = doc().querySelector('.face');
    if (face) face.src = faceFor(status);
    const box = doc().querySelector('.box');
    if (box) {
      box.classList.remove(...STATUS_CLASSES.map(s => 'status-' + s));
      box.classList.add('status-' + (STATUS_CLASSES.includes(status) ? status : 'unknown'));
    }
  }

  function apply(data) {
    if (data.currentTime) state.serverOffset = Date.now() - data.currentTime;
    const sensors = data.sensors || {};
    const flower = data.flowerStatusSensor;
    updateFlower(sensors[flower] ? sensors[flower].status : 'unknown');
    rows().forEach(row => {
      row.classList.toggle('active', row.dataset.sensor === flower);
      const d = sensors[row.dataset.sensor];
      if (d) updateRow(row, d);
    });
  }

  function showError() {
    updateFlower('error');
  }

  function poll() {
    if (state.inFlight) return Promise.resolve();
    state.inFlight = true;
    return fetch('/getLatestValues')
      .then(r => {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      })
      .then(d => {
        state.failures = 0;
        apply(d);
        return d;
      })
      .catch(() => {
        state.failures++;
        if (state.failures >= MAX_FAILURES) showError();
      })
      .then(d => {
        state.inFlight = false;
        return d;
      });
  }

  function tickCountdowns() {
    releaseStale();
    rows().forEach(row => {
      if (suppressed(row) || !row.dataset.lastMeasurement) return;
      row.querySelector('.interval span').textContent = intervalText(
        parseInt(row.dataset.lastMeasurement, 10), parseInt(row.dataset.measurementInterval, 10), serverNow());
    });
  }

  function startFastPolls() {
    state.fastStep = 0;
    clearTimeout(state.fastTimer);
    const next = () => {
      if (state.fastStep >= FAST_POLL_DELAYS_MS.length) return;
      state.fastTimer = setTimeout(() => {
        state.fastStep++;
        poll().then(() => {
          if (doc().querySelector('.sensor.measuring')) next();
        });
      }, FAST_POLL_DELAYS_MS[state.fastStep]);
    };
    next();
  }

  function triggerMeasurement(row) {
    const id = row.dataset.sensorId;
    if (!id || row.classList.contains('measuring')) return Promise.resolve();
    if (state.cooldown[id] && Date.now() < state.cooldown[id]) return Promise.resolve();
    const group = rowsOf(id);
    group.forEach(startMeasuring);
    return fetch('/api/v1/measure', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ sensor: id })
    })
      .then(r => {
        state.cooldown[id] = Date.now() + MEASURE_COOLDOWN_MS;
        if (r.ok) {
          startFastPolls();
          return;
        }
        group.forEach(x => {
          stopMeasuring(x);
          setNote(x, r.status === 429 ? '(bitte warten)' : '(Fehler)', 3000);
        });
      })
      .catch(() => group.forEach(x => {
        stopMeasuring(x);
        setNote(x, '(Fehler)', 3000);
      }));
  }

  if (typeof document !== 'undefined' && global.Footer) {
    document.addEventListener('DOMContentLoaded', () => {
      global.Footer.onSystem(s => {
        renderCards(s.channels || []);
        poll();
      });
      setInterval(() => { if (document.visibilityState !== 'hidden') poll(); }, POLL_MS);
      setInterval(() => { if (document.visibilityState !== 'hidden') tickCountdowns(); }, 1000);
      document.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'visible') poll();
      });
    });
  }

  global.Start = { faceFor, intervalText, parseMatrix, pivotInCard, renderCards, apply, poll, triggerMeasurement, tickCountdowns,
                   state, FAST_POLL_DELAYS_MS };
})(typeof window !== 'undefined' ? window : globalThis);
