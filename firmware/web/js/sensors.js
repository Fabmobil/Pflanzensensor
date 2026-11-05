/**
 * @file sensors.js
 * @brief Seite Sensoren: Blumengesicht, LED-Ampel, je Sensor eine Karte
 *
 * Neu und schlanker als in der Firmware 2.x: alles kommt aus GET /api/v1/sensors
 * und geht als Teil-JSON an PATCH /api/v1/sensors. Jedes Feld speichert für
 * sich nach 1 s Ruhe (B-50). Werte werden alle 5 s aufgefrischt.
 */
(function (global) {
  'use strict';
  const { el, toast } = global.UI;
  const HALF_LIVES = [[21600, '6 Stunden'], [43200, '12 Stunden'], [86400, '1 Tag'], [259200, '3 Tage'],
                      [604800, '7 Tage'], [2592000, '30 Tage']];
  const WIKI = 'https://github.com/Fabmobil/Pflanzensensor/wiki/automatische-Kalibrierung';
  const HEALTH = { ok: 'OK', degraded: 'Fehler, neuer Versuch läuft', failed: 'dauerhaft fehlerhaft' };

  const live = new Map();  // key → {value, status, obs, raw, rawObs, card}

  /** Teil-JSON für ein Kanalfeld */
  function channelPatch(sensorId, index, field, value) {
    return { [sensorId]: { channels: { [String(index)]: { [field]: value } } } };
  }

  function sensorPatch(sensorId, field, value) {
    return { [sensorId]: { [field]: value } };
  }

  function save(patch) {
    return global.Api.patch('/api/v1/sensors', patch);
  }

  function numberField(value, attrs) {
    return el('input', Object.assign({ type: 'number', step: 'any', value: value === null ? '' : value }, attrs || {}));
  }

  /** Zeile einer Zwei-Spalten-Tabelle: Bezeichnung | Inhalt */
  function kv(label, ...content) {
    return [el('span', { class: 'kv-label', text: label }), el('div', { class: 'kv-value' }, content)];
  }

  function smallButton(text) {
    return el('button', { class: 'button button-secondary button-small', type: 'button', text });
  }

  function checkbox(checked, text, extra) {
    const box = el('input', { type: 'checkbox', checked });
    return { box, label: el('label', { class: 'check' }, [box, el('span', { text })].concat(extra || [])) };
  }

  function channelCard(sensor, ch) {
    const id = sensor.id;
    const bind = (field, name, read) => global.UI.bindAutosave(field, v => save(channelPatch(id, ch.index, name, v)), read ? { read } : undefined);
    const name = el('input', { type: 'text', class: 'channel-name', maxlength: '24', value: ch.name, 'aria-label': 'Name' });
    bind(name, 'name');
    const enabled = checkbox(ch.enabled, 'aktiv');
    bind(enabled.box, 'enabled');
    const valueEl = el('span', { class: 'readonly-value' });
    const obsEl = el('span', { class: 'readonly-value' });
    const resetObs = smallButton('Zurücksetzen');
    resetObs.addEventListener('click', () => global.Api.post(`/api/v1/sensors/${id}/${ch.index}/reset-extrema`, { raw: false })
      .then(() => toast('Min/Max zurückgesetzt')));

    // Schwellen: 4 Zahlenfelder + Regler
    const inputs = {};
    const thresholdRow = el('div', { class: 'threshold-inputs' + (ch.oneSided ? ' one-sided' : '') });
    global.ThresholdSlider.KEYS.forEach(k => {
      const f = numberField(ch[k]);
      inputs[k] = f;
      bind(f, k);
      const hidden = ch.oneSided && (k === 'yellowLow' || k === 'greenLow');
      const label = { yellowLow: 'Gelb unten', greenLow: 'Grün unten', greenHigh: 'Grün oben', yellowHigh: 'Gelb oben' }[k];
      const wrap = el('label', { class: 'threshold-row threshold-' + k }, [el('span', { text: label }), f]);
      if (hidden) wrap.style.display = 'none';
      thresholdRow.appendChild(wrap);
    });
    const slider = global.ThresholdSlider.create(ch, (limits, key) => {
      inputs[key].value = limits[key];
      inputs[key].dispatchEvent(new Event('input'));
    });
    Object.values(inputs).forEach(f => f.addEventListener('input', () => {
      const l = {};
      global.ThresholdSlider.KEYS.forEach(k => { l[k] = Number(inputs[k].value); });
      slider.set(l);
    }));

    const parts = [
      el('div', { class: 'name-row' }, [enabled.label, name]),
      el('div', { class: 'kv' }, [].concat(
        kv('Aktuell', valueEl),
        kv('Beobachtet', obsEl, resetObs))),
      el('h4', { text: ch.oneSided ? 'Schwellen (nur obere Grenzen)' : 'Schwellen' }),
      thresholdRow, slider.element
    ];

    let rawEl = null, rawObsEl = null;
    if (ch.hasRaw) {
      const inv = checkbox(ch.inverted, 'Skala invertieren (hoher Rohwert = 0 %)');
      bind(inv.box, 'inverted');
      const rmin = numberField(ch.rawMin, { step: '1', min: '0', max: '1023', disabled: ch.autocal, 'aria-label': 'Rohwert für 0 %' });
      const rmax = numberField(ch.rawMax, { step: '1', min: '0', max: '1023', disabled: ch.autocal, 'aria-label': 'Rohwert für 100 %' });
      bind(rmin, 'rawMin');
      bind(rmax, 'rawMax');
      const cal = checkbox(ch.autocal, 'Automatische Kalibrierung',
        [el('a', { href: WIKI, target: '_blank', rel: 'noopener', text: '(Wiki)' })]);
      bind(cal.box, 'autocal');
      const half = el('select', { class: 'half-life' }, HALF_LIVES.map(([v, t]) => el('option', { value: String(v), text: t, selected: v === ch.halfLife })));
      bind(half, 'halfLife', f => Number(f.value));
      const halfRow = el('div', { class: 'kv' }, kv('Halbwertszeit', half));
      const autocalChanged = () => {
        rmin.disabled = rmax.disabled = cal.box.checked;
        halfRow.style.display = cal.box.checked ? '' : 'none';
      };
      cal.box.addEventListener('change', autocalChanged);
      autocalChanged();
      rawEl = el('span', { class: 'readonly-value' });
      rawObsEl = el('span', { class: 'readonly-value' });
      const resetRaw = smallButton('Zurücksetzen');
      resetRaw.addEventListener('click', () => global.Api.post(`/api/v1/sensors/${id}/${ch.index}/reset-extrema`, { raw: true })
        .then(() => toast('Roh-Extrema zurückgesetzt')));
      parts.push(el('div', { class: 'inverted-section' }, [
        el('h4', { text: 'Analog' }),
        inv.label,
        el('div', { class: 'kv' }, kv('Rohgrenzen', el('span', { class: 'minmax-section' }, [rmin, ' bis ', rmax]))),
        cal.label,
        halfRow,
        el('div', { class: 'kv' }, [].concat(
          kv('Rohwert', rawEl),
          kv('Roh beobachtet', rawObsEl, resetRaw)))
      ]));
    }
    const card = el('div', { class: 'measurement-card', 'data-key': ch.key }, parts);
    live.set(ch.key, { card, valueEl, obsEl, rawEl, rawObsEl, slider, unit: ch.unit, last: null });
    return card;
  }

  function sensorCard(s) {
    const enabled = checkbox(s.enabled, 'Sensor aktiviert');
    global.UI.bindAutosave(enabled.box, v => save(sensorPatch(s.id, 'enabled', v)).then(() => card.classList.toggle('disabled', !v)), { delay: 0 });
    const interval = numberField(s.interval, { step: '1', min: '10', max: '3600', 'aria-label': 'Messintervall in Sekunden' });
    global.UI.bindAutosave(interval, v => save(sensorPatch(s.id, 'interval', v)));
    const measure = el('button', { class: 'button button-primary', type: 'button', text: 'Messen' });
    measure.addEventListener('click', () => global.Api.post(`/api/v1/sensors/${s.id}/measure`).then(() => toast('Messung gestartet')));
    const status = el('span', { class: 'value', 'data-status': s.id });
    const head = [
      el('div', { class: 'sensor-id-title' }, [el('h2', { text: s.id }), el('div', { class: 'status-row' }, [status])]),
      el('div', { class: 'sensor-controls' }, [
        enabled.label,
        el('label', { class: 'interval' }, ['Messintervall ', interval, ' s']),
        measure
      ])
    ];
    if (s.tare) {
      const tare = el('button', { class: 'button button-warning', type: 'button', text: 'Tarieren' });
      tare.addEventListener('click', () => global.Api.post(`/api/v1/sensors/${s.id}/tare`).then(() => toast('Nullpunkt wird bei der nächsten Messung übernommen')));
      head.push(el('p', {}, [tare, el('span', { class: 'hint', text: ' Waage leer lassen.' })]));
    }
    const card = el('div', { class: 'card sensor-card' + (s.enabled ? '' : ' disabled'), 'data-sensor': s.id }, head.concat(s.channels.map(ch => channelCard(s, ch))));
    return card;
  }

  function sensorStatusText(s) {
    if (s.warmupMs > 0) return 'Aufwärmen (noch ' + Math.ceil(s.warmupMs / 1000) + ' s)';
    if (s.active) return 'misst …';
    let t = HEALTH[s.health] || s.health;
    if (s.failures) t += ' – ' + s.failures + ' Fehlschläge in Folge';
    return t;
  }

  function refreshValues(data) {
    data.sensors.forEach(s => {
      const st = document.querySelector(`[data-status="${s.id}"]`);
      if (st) st.textContent = sensorStatusText(s);
      s.channels.forEach(ch => {
        const l = live.get(ch.key);
        if (!l) return;
        l.valueEl.textContent = global.UI.formatValue(ch.value, ch.unit) + ' (' + global.UI.statusText(ch.status) + ')';
        l.valueEl.dataset.status = ch.status;
        l.obsEl.textContent = global.UI.formatValue(ch.obsMin, ch.unit) + ' … ' + global.UI.formatValue(ch.obsMax, ch.unit);
        if (l.rawEl) l.rawEl.textContent = ch.raw >= 0 ? String(ch.raw) : '--';
        if (l.rawObsEl) l.rawObsEl.textContent = ch.rawObsMin === null ? '--' : ch.rawObsMin + ' … ' + ch.rawObsMax;
        l.slider.update(ch);
        const sig = s.lastMeasurement;
        if (l.last !== null && l.last !== sig) {
          l.card.classList.remove('flash');
          void l.card.offsetWidth;
          l.card.classList.add('flash');
        }
        l.last = sig;
      });
    });
  }

  function fillChannelSelect(sel, channels, current) {
    sel.textContent = '';
    channels.forEach(c => sel.appendChild(el('option', { value: c.key, text: c.name + ' (' + c.key + ')', selected: c.key === current })));
  }

  function wire() {
    global.Api.get('/api/v1/config').then(cfg => {
      fillChannelSelect(document.getElementById('flower'), cfg.channels, cfg.config.general.flowerChannel);
      fillChannelSelect(document.getElementById('ledChannel'), cfg.channels, cfg.config.led.channel);
      if (!cfg.features.led) document.getElementById('ledCard').style.display = 'none';
      if (!cfg.features.display) document.querySelectorAll('[data-feature="display"]').forEach(o => o.remove());
      return global.ConfigBind.bindConfig(document);
    }).then(() => {
      const mode = document.getElementById('ledMode');
      const showChannel = () => {
        document.getElementById('ledChannelGroup').style.display = mode.value === '2' ? '' : 'none';
      };
      mode.addEventListener('change', showChannel);
      showChannel();
    });
    global.Api.get('/api/v1/sensors').then(data => {
      const box = document.getElementById('sensorCards');
      data.sensors.forEach(s => box.appendChild(sensorCard(s)));
      refreshValues(data);
    });
    setInterval(() => {
      if (document.visibilityState === 'hidden') return;
      global.Api.get('/api/v1/sensors', { quiet: true }).then(refreshValues).catch(() => {});
    }, 5000);
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Sensors = { channelPatch, sensorPatch, sensorStatusText };
})(typeof window !== 'undefined' ? window : globalThis);
