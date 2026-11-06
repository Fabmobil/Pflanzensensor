/**
 * @file sensors.js
 * @brief Seite Sensoren: Blumengesicht, LED-Ampel, je Sensor eine Karte
 *
 * Neu und schlanker als in der Firmware 2.x: alles kommt aus GET /api/v1/sensors
 * und geht als Teil-JSON an PATCH /api/v1/sensors. Jedes Feld speichert für
 * sich nach 1 s Ruhe (B-50). Werte werden alle 5 s aufgefrischt; Felder, die das
 * Gerät selbst ändert (Kalibrierpunkte, Nasspunkt nach dem Gießen), ziehen nach,
 * solange niemand darin tippt.
 *
 * Kalibrieren: Analogkanäle und der Wasservorrat der Waage übernehmen den
 * aktuellen Wert als 0 % bzw. 100 %. Die Waage kalibriert ihre Skalierung mit
 * einem bekannten Gewicht. Ein Bodenfeuchtesensor im selben Topf kann an der
 * Waage lernen und bekommt daraus Rohgrenzen vorgeschlagen.
 */
(function (global) {
  'use strict';
  const { el, toast } = global.UI;
  const HALF_LIVES = [[21600, '6 Stunden'], [43200, '12 Stunden'], [86400, '1 Tag'], [259200, '3 Tage'],
                      [604800, '7 Tage'], [2592000, '30 Tage']];
  const WIKI = 'https://github.com/Fabmobil/Pflanzensensor/wiki/automatische-Kalibrierung';
  const HEALTH = { ok: 'OK', degraded: 'Fehler, neuer Versuch läuft', failed: 'dauerhaft fehlerhaft' };

  const live = new Map();  // key → {value, status, obs, raw, rawObs, card}
  const sensorLive = new Map();  // Sensor-ID → {sync, update}

  /** Feld merkt sich Eingaben, bis das Gerät den Wert bestätigt */
  function track(field) {
    const mark = () => { field.dataset.dirty = '1'; };
    field.addEventListener('input', mark);
    field.addEventListener('change', mark);
    return field;
  }

  /** Wert vom Gerät übernehmen, außer im Feld wird gerade getippt */
  function sync(field, v) {
    if (!field || v === undefined) return;
    const isBox = field.type === 'checkbox';
    const cur = isBox ? field.checked : (field.value === '' ? null : Number(field.value));
    const same = isBox || cur === null || v === null ? cur === v
      : Math.abs(cur - v) <= 1e-4 * Math.max(1, Math.abs(v));
    if (field.dataset.dirty) {
      if (same) delete field.dataset.dirty; // gespeichert
      return;
    }
    if (same || (typeof document !== 'undefined' && document.activeElement === field)) return;
    if (isBox) field.checked = v;
    else field.value = v === null ? '' : String(v);
    field.dispatchEvent(new Event('sync')); // nicht change: das würde wieder speichern
  }

  function post(path, body, message) {
    return global.Api.post(path, body).then(() => { if (message) toast(message); }).catch(() => {});
  }

  /** "vor 5 min", "vor 3 h" */
  function ago(ms) {
    const m = Math.max(0, Math.round(ms / 60000));
    if (m < 90) return 'vor ' + m + ' min';
    const h = Math.round(m / 60);
    return h < 48 ? 'vor ' + h + ' h' : 'vor ' + Math.round(h / 24) + ' Tagen';
  }

  function fmt(v, digits) {
    return v === null || v === undefined || isNaN(v) ? '--' : Number(v).toFixed(digits);
  }

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
    bind(track(enabled.box), 'enabled');
    const synced = { enabled: enabled.box };
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
      bind(track(inv.box), 'inverted');
      const rmin = numberField(ch.rawMin, { step: '1', min: '0', max: '1023', disabled: ch.autocal, 'aria-label': 'Rohwert für 0 %' });
      const rmax = numberField(ch.rawMax, { step: '1', min: '0', max: '1023', disabled: ch.autocal, 'aria-label': 'Rohwert für 100 %' });
      bind(track(rmin), 'rawMin');
      bind(track(rmax), 'rawMax');
      const cal = checkbox(ch.autocal, 'Automatische Kalibrierung',
        [el('a', { href: WIKI, target: '_blank', rel: 'noopener', text: '(Wiki)' })]);
      bind(track(cal.box), 'autocal');
      Object.assign(synced, { inverted: inv.box, rawMin: rmin, rawMax: rmax, autocal: cal.box });
      const half = el('select', { class: 'half-life' }, HALF_LIVES.map(([v, t]) => el('option', { value: String(v), text: t, selected: v === ch.halfLife })));
      bind(half, 'halfLife', f => Number(f.value));
      const halfRow = el('div', { class: 'kv' }, kv('Halbwertszeit', half));
      const autocalChanged = () => {
        rmin.disabled = rmax.disabled = cal.box.checked;
        halfRow.style.display = cal.box.checked ? '' : 'none';
      };
      cal.box.addEventListener('change', autocalChanged);
      cal.box.addEventListener('sync', autocalChanged);
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
    let fit = null;
    if (ch.calibrate) {
      const cal = calibrationSection(sensor, ch, synced);
      fit = cal.fit;
      parts.push(cal.element);
    }
    const card = el('div', { class: 'measurement-card', 'data-key': ch.key }, parts);
    live.set(ch.key, { card, valueEl, obsEl, rawEl, rawObsEl, slider, unit: ch.unit, last: null, synced, fit });
    return card;
  }

  /** Kalibrierpunkte 0 %/100 % und (Analog mit Waage) Lernen an der Waage */
  function calibrationSection(sensor, ch, synced) {
    const id = sensor.id;
    const water = id === 'HX711';
    const point = (pct) => {
      const b = smallButton('Jetzt = ' + pct + ' %');
      b.addEventListener('click', () => post(`/api/v1/sensors/${id}/${ch.index}/calibrate`, { percent: pct },
        (water ? (pct ? 'Nassgewicht' : 'Trockengewicht') : 'Rohgrenze') + ' übernommen'));
      return b;
    };
    const parts = [
      el('h4', { text: 'Kalibrieren' }),
      el('p', { class: 'calibrate-points' }, [point(0), ' ', point(100)]),
      el('p', { class: 'hint', text: water
        ? '0 % = die Pflanze braucht Wasser, 100 % = gegossen und abgetropft. Übernimmt das aktuelle Gewicht als Trocken- bzw. Nassgewicht.'
        : 'Übernimmt den letzten Rohwert als Grenze für 0 % bzw. 100 % und schaltet die automatische Kalibrierung aus.' })
    ];
    let fit = null;
    if (ch.fit !== undefined) {
      const learn = checkbox(ch.learn, 'An der Waage lernen');
      global.UI.bindAutosave(track(learn.box), v => save(channelPatch(id, ch.index, 'learn', v)), { delay: 0 });
      synced.learn = learn.box;
      const table = el('div', { class: 'fit-bins' });
      const suggestion = el('p', { class: 'fit-suggestion' });
      const apply = smallButton('Vorschlag übernehmen');
      apply.addEventListener('click', () => post(`/api/v1/sensors/${id}/${ch.index}/fit-apply`, undefined, 'Rohgrenzen übernommen'));
      const reset = smallButton('Lerndaten löschen');
      reset.addEventListener('click', () => post(`/api/v1/sensors/${id}/${ch.index}/fit-reset`, undefined, 'Lerndaten gelöscht'));
      parts.push(learn.label,
        el('p', { class: 'hint', text: 'Steckt der Sensor im Topf auf der Waage, sammelt das Gerät seine Rohwerte je 10 % Wasservorrat (nicht beim Abtropfen nach dem Gießen). Nach einem Trocknungszyklus schlägt es Rohgrenzen vor. R² nahe 1: der Sensor ist linear.' }),
        table, suggestion, el('p', {}, [apply, ' ', reset]));
      fit = { table, suggestion, apply };
    }
    return { element: el('div', { class: 'calibration-section' }, parts), fit };
  }

  function renderFit(f, data) {
    if (!f || !data) return;
    f.table.textContent = '';
    data.bins.forEach((b, i) => {
      if (!b) return;
      f.table.appendChild(el('span', { class: 'fit-bin', text: `${i * 10}–${i * 10 + 10} %: ${Math.round(b.raw)} (${b.n})` }));
    });
    if (!f.table.children.length) f.table.appendChild(el('span', { class: 'hint', text: 'Noch keine Lerndaten.' }));
    const s = data.suggest;
    f.suggestion.textContent = s
      ? `Vorschlag: Rohgrenzen ${s.rawMin} bis ${s.rawMax}${s.inverted ? ', invertiert' : ''} (R² ${fmt(s.r2, 2)} aus ${s.bins} Bereichen)`
      : 'Noch kein Vorschlag: nötig sind drei Bereiche mit je drei Messungen, die zusammen 30 % Wasservorrat überspannen.';
    f.apply.disabled = !s;
  }

  /** Waage: Skalierung mit Referenzgewicht, Wasservorrat, Gießen */
  function scaleSection(s) {
    const field = (name, attrs) => {
      const f = track(numberField(s[name], attrs));
      global.UI.bindAutosave(f, v => save(sensorPatch(s.id, name, v)));
      return f;
    };
    const scale = field('scale', { 'aria-label': 'Rohwert je Gramm' });
    const grams = numberField('', { step: 'any', min: '0', placeholder: 'g', 'aria-label': 'Referenzgewicht in Gramm' });
    const ref = smallButton('1. Ausgangswert merken');
    ref.addEventListener('click', () => post(`/api/v1/sensors/${s.id}/scale-reference`, undefined, 'Ausgangswert wird bei der nächsten Messung gemerkt'));
    const calib = smallButton('2. Kalibrieren');
    calib.addEventListener('click', () => {
      const g = Number(grams.value);
      if (!(g > 0)) { toast('Bekanntes Gewicht in Gramm eintragen', 'warning'); return; }
      post(`/api/v1/sensors/${s.id}/scale`, { grams: g }, 'Skalierung wird bei der nächsten Messung berechnet');
    });
    const scaleStatus = el('span', { class: 'scale-status' });
    const dry = field('dry', { 'aria-label': 'Trockengewicht in Gramm' });
    const wet = field('wet', { 'aria-label': 'Nassgewicht in Gramm' });
    const watering = field('watering', { step: '1', min: '0', max: '10000', 'aria-label': 'Gießen erkennen ab Gramm' });
    const autoWet = checkbox(s.autoWet, 'Nasspunkt nach dem Gießen nachführen');
    global.UI.bindAutosave(track(autoWet.box), v => save(sensorPatch(s.id, 'autoWet', v)), { delay: 0 });
    const waterStatus = el('span', { class: 'watering-status' });
    const element = el('div', { class: 'scale-section' }, [
      el('h4', { text: 'Waage kalibrieren' }),
      el('div', { class: 'kv' }, [].concat(
        kv('Skalierung', scale, el('span', { class: 'hint', text: ' Rohwert je g' })),
        kv('Referenz', ref, ' ', grams, ' g ', calib),
        kv('Stand', scaleStatus))),
      el('p', { class: 'hint', text: 'Der Topf darf stehen bleiben: Ausgangswert merken, ein bekanntes Gewicht (z. B. eine gewogene Wasserflasche) dazustellen, Gramm eintragen, Kalibrieren. Ohne Ausgangswert zählt der Nullpunkt (Tara bei leerer Waage).' }),
      el('h4', { text: 'Wasservorrat' }),
      el('div', { class: 'kv' }, [].concat(
        kv('Trocken (0 %)', dry, ' g'),
        kv('Nass (100 %)', wet, ' g'),
        kv('Gießen ab', watering, ' g Anstieg (0 = aus)'),
        kv('Gegossen', waterStatus))),
      autoWet.label,
      el('p', { class: 'hint', text: 'Nach dem Gießen wartet das Gerät 60 min aufs Abtropfen. Ist der Topf dann schwerer als das Nassgewicht, wird es angehoben (die Pflanze wächst).' })
    ]);
    const synced = { scale, dry, wet, watering, autoWet: autoWet.box };
    sensorLive.set(s.id, {
      sync: (x) => Object.entries(synced).forEach(([k, f]) => sync(f, x[k])),
      update: (x, now) => {
        scaleStatus.textContent = x.scalePending ? 'wartet auf die nächste Messung …'
          : x.scaleRef ? 'Ausgangswert gemerkt – jetzt Gewicht auflegen und kalibrieren' : '–';
        const w = x.lastWatering || {};
        waterStatus.textContent = w.at === null || w.at === undefined ? 'seit dem Start nicht erkannt'
          : ago(now - w.at) + ' (+' + fmt(w.rise, 0) + ' g)' + (w.settling ? ', tropft ab …' : '');
      }
    });
    return element;
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
    if (s.id === 'HX711') head.push(scaleSection(s));
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
      const sl = sensorLive.get(s.id);
      if (sl) {
        sl.sync(s);
        sl.update(s, data.now);
      }
      s.channels.forEach(ch => {
        const l = live.get(ch.key);
        if (!l) return;
        l.valueEl.textContent = global.UI.formatValue(ch.value, ch.unit) + ' (' + global.UI.statusText(ch.status) + ')';
        l.valueEl.dataset.status = ch.status;
        l.obsEl.textContent = global.UI.formatValue(ch.obsMin, ch.unit) + ' … ' + global.UI.formatValue(ch.obsMax, ch.unit);
        if (l.rawEl) l.rawEl.textContent = ch.raw >= 0 ? String(ch.raw) : '--';
        if (l.rawObsEl) l.rawObsEl.textContent = ch.rawObsMin === null ? '--' : ch.rawObsMin + ' … ' + ch.rawObsMax;
        l.slider.update(ch);
        Object.entries(l.synced).forEach(([k, f]) => sync(f, ch[k]));
        renderFit(l.fit, ch.fit);
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
  global.Sensors = { channelPatch, sensorPatch, sensorStatusText, ago };
})(typeof window !== 'undefined' ? window : globalThis);
