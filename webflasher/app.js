/**
 * @file app.js
 * @brief Webflasher-Seite: Version, Gerät, Sensoren/Pins, Einstellungen → Manifest
 */
(function () {
  'use strict';
  const H = window.Hardware, P = window.Provision, F = window.Flasher;
  const $ = id => document.getElementById(id);

  /** Pins zur Auswahl je Chip: [GPIO, Beschriftung] */
  const PIN_CHOICES = {
    ESP8266: [[16, 'D0 (GPIO16)'], [5, 'D1 (GPIO5)'], [4, 'D2 (GPIO4)'], [0, 'D3 (GPIO0)'], [2, 'D4 (GPIO2)'],
      [14, 'D5 (GPIO14)'], [12, 'D6 (GPIO12)'], [13, 'D7 (GPIO13)'], [15, 'D8 (GPIO15)'], [3, 'RX (GPIO3)'],
      [1, 'TX (GPIO1)'], [17, 'A0']],
    ESP32: [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39]
      .map(n => [n, 'GPIO' + n + (n >= 34 ? ' (nur Eingang)' : '')])
  };

  /** Sensoren und Peripherie mit Optionen und Pins */
  const PARTS = [
    { key: 'analog', label: 'Analoge Sensoren (Licht, Bodenfeuchte)',
      opts: [{ f: 'analogChannels', label: 'Kanäle', type: 'number', min: 1, max: 8 },
        { f: 'multiplexer', label: 'Multiplexer (CD4051)', type: 'checkbox' }],
      pins: [['adc', 'Analog-Eingang'], ['muxA', 'Mux A'], ['muxB', 'Mux B'], ['muxC', 'Mux C']] },
    { key: 'dht', label: 'DHT (Lufttemperatur und -feuchte)',
      opts: [{ f: 'dhtType', label: 'Typ', type: 'select', choices: [[11, 'DHT11'], [22, 'DHT22']] }],
      pins: [['dht', 'Daten']] },
    { key: 'ds18b20', label: 'DS18B20 (Temperaturfühler, OneWire)',
      opts: [{ f: 'ds18b20Max', label: 'Fühler', type: 'number', min: 1, max: 8 }], pins: [['oneWire', 'OneWire']] },
    { key: 'bmp280', label: 'BMP280 (Luftdruck, I²C)',
      opts: [{ f: 'bmp280Address', label: 'Adresse', type: 'select', choices: [[0x76, '0x76'], [0x77, '0x77']] }], pins: [] },
    { key: 'sds011', label: 'SDS011 (Feinstaub)', opts: [], pins: [['sdsRx', 'RX'], ['sdsTx', 'TX']] },
    { key: 'mhz19', label: 'MH-Z19 (CO₂, PWM)',
      opts: [{ f: 'mhz19Range', label: 'Messbereich', type: 'select', choices: [[2000, '0–2000 ppm'], [5000, '0–5000 ppm']] }],
      pins: [['mhz19Pwm', 'PWM']] },
    { key: 'hx711', label: 'HX711 (Waage)', opts: [], pins: [['hxSck', 'SCK'], ['hxDout', 'DOUT']] },
    { key: 'serialReceiver', label: 'Serieller Empfänger (Durchfluss, 7 Werte)',
      opts: [{ f: 'serialBaud', label: 'Baud', type: 'select', choices: [[9600, '9600'], [19200, '19200'], [38400, '38400'], [115200, '115200']] }],
      pins: [['serialRx', 'RX'], ['serialTx', 'TX']] },
    { key: 'display', label: 'Display SSD1306 (I²C)',
      opts: [{ f: 'displayAddress', label: 'Adresse', type: 'select', choices: [[0x3C, '0x3C'], [0x3D, '0x3D']] }], pins: [] },
    { key: 'ledTraffic', label: 'LED-Ampel', opts: [], pins: [['ledGreen', 'grün'], ['ledYellow', 'gelb'], ['ledRed', 'rot']] }
  ];

  const state = { releases: [], release: null, mode: 'pcb_v1', hw: H.preset('pcb_v1'), urls: [] };

  function el(tag, attrs, children) {
    const e = document.createElement(tag);
    Object.entries(attrs || {}).forEach(([k, v]) => {
      if (k === 'text') e.textContent = v;
      else if (k === 'class') e.className = v;
      else if (v === true) e.setAttribute(k, '');
      else if (v !== false && v !== null && v !== undefined) e.setAttribute(k, v);
    });
    (children || []).forEach(c => e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c));
    return e;
  }

  function pinSelect(field) {
    const s = el('select', { 'data-pin': field });
    s.appendChild(el('option', { value: '-1', text: '– nicht belegt –' }));
    PIN_CHOICES[state.hw.chip].forEach(([n, t]) => s.appendChild(el('option', { value: String(n), text: t })));
    s.value = String(state.hw.pins[field]);
    if (s.value !== String(state.hw.pins[field])) { // Pin aus der Voreinstellung nicht in der Liste
      s.appendChild(el('option', { value: String(state.hw.pins[field]), text: 'GPIO' + state.hw.pins[field] }));
      s.value = String(state.hw.pins[field]);
    }
    s.addEventListener('change', () => { state.hw.pins[field] = Number(s.value); changed(); });
    return el('label', { class: 'pin' }, [s]);
  }

  function optionField(o) {
    const f = state.hw.features;
    let input;
    if (o.type === 'checkbox') {
      input = el('input', { type: 'checkbox', checked: !!f[o.f] });
      input.addEventListener('change', () => { f[o.f] = input.checked; changed(); });
      return el('label', { class: 'opt' }, [input, ' ' + o.label]);
    }
    if (o.type === 'select') {
      input = el('select', {});
      o.choices.forEach(([v, t]) => input.appendChild(el('option', { value: String(v), text: t })));
      input.value = String(f[o.f]);
      input.addEventListener('change', () => { f[o.f] = Number(input.value); changed(); });
    } else {
      input = el('input', { type: 'number', min: String(o.min), max: String(o.max), value: String(f[o.f] || o.min) });
      input.addEventListener('input', () => { f[o.f] = Number(input.value); changed(); });
    }
    return el('label', { class: 'opt' }, [o.label + ' ', input]);
  }

  function renderSensors() {
    const box = $('sensors');
    box.textContent = '';
    const f = state.hw.features;
    PARTS.forEach(part => {
      const on = el('input', { type: 'checkbox', checked: !!f[part.key] });
      on.addEventListener('change', () => {
        f[part.key] = on.checked;
        if (part.key === 'ds18b20' && on.checked && !f.ds18b20Max) f.ds18b20Max = 1;
        if (part.key === 'analog' && on.checked && !f.analogChannels) f.analogChannels = 1;
        renderSensors();
        changed();
      });
      const head = el('label', { class: 'part-head' }, [on, ' ' + part.label]);
      const body = el('div', { class: 'part-body' });
      if (f[part.key]) {
        part.opts.forEach(o => body.appendChild(optionField(o)));
        part.pins.forEach(([field, label]) => {
          if ((field.startsWith('mux')) && !f.multiplexer) return;
          const l = pinSelect(field);
          l.insertBefore(document.createTextNode(label + ' '), l.firstChild);
          body.appendChild(l);
        });
      }
      box.appendChild(el('div', { class: 'part' + (f[part.key] ? ' on' : '') }, [head, body]));
    });
    if (f.display || f.bmp280) {
      const bus = el('div', { class: 'part-body' });
      [['sda', 'SDA'], ['scl', 'SCL']].forEach(([field, label]) => {
        const l = pinSelect(field);
        l.insertBefore(document.createTextNode(label + ' '), l.firstChild);
        bus.appendChild(l);
      });
      box.appendChild(el('div', { class: 'part on' }, [el('div', { class: 'part-head', text: 'I²C-Bus (Display, BMP280)' }), bus]));
    }
  }

  function settingsFromForm() {
    return {
      deviceName: $('deviceName').value, adminPassword: $('adminPassword').value,
      wifiSsid: $('wifiSsid').value, wifiPassword: $('wifiPassword').value,
      staticIp: { ip: $('ipIp').value, gateway: $('ipGateway').value, subnet: $('ipSubnet').value || '255.255.255.0', dns1: $('ipDns1').value }
    };
  }

  function isPreset() {
    const p = H.preset(state.mode);
    return JSON.stringify(p.features) === JSON.stringify(state.hw.features) &&
      JSON.stringify(p.pins) === JSON.stringify(state.hw.pins);
  }

  /** Alles neu prüfen und das Manifest setzen */
  function changed() {
    state.urls.forEach(u => URL.revokeObjectURL(u));
    state.urls = [];
    const keep = state.mode.startsWith('keep:');
    const chip = keep ? state.mode.slice(5) : state.hw.chip;
    let problem = '';
    if (!state.release) problem = 'Kein Release ausgewählt.';
    let provisionUrl = null;
    if (!keep) {
      state.hw.name = isPreset() ? state.mode : 'eigene';
      const r = H.check(chip, state.hw.features, state.hw.pins);
      const n = H.channelCount(state.hw.features);
      $('hwStatus').textContent = (r.problem === 'ok' ? '✓ ' : '✗ ') + H.describe(r) + ' ' + n + ' von höchstens ' +
        H.maxChannels(chip) + ' Messkanälen' + (state.hw.name === 'eigene' ? ' (angepasst)' : '') + '.';
      $('hwStatus').className = 'status ' + (r.problem === 'ok' ? 'ok' : 'bad');
      if (r.problem !== 'ok') problem = 'Bitte zuerst die Hardware-Angabe korrigieren.';
      const s = settingsFromForm();
      const sp = F.settingsProblem(s);
      $('settingsStatus').textContent = sp;
      $('settingsStatus').className = 'status ' + (sp ? 'bad' : '');
      if (sp) problem = problem || sp;
      if (!problem) {
        const hw = Object.assign({}, state.hw, { staticIp: s.staticIp.ip.trim() ? s.staticIp : null });
        provisionUrl = URL.createObjectURL(new Blob([P.encode(hw, F.cleanSettings(s))], { type: 'application/octet-stream' }));
        state.urls.push(provisionUrl);
      }
    } else {
      $('hwStatus').textContent = 'Nur die Firmware wird geschrieben; die Hardware-Angabe auf dem Gerät bleibt ' +
        '(ohne Angabe: Voreinstellung ' + (chip === 'ESP8266' ? 'Pflanzensensor-PCB' : 'ESP32-DevKit') + ').';
      $('hwStatus').className = 'status';
    }
    $('customize').hidden = keep;
    $('settingsCard').hidden = keep;
    if (!problem && state.release && !(state.release.parts[chip] || []).length)
      problem = 'Version ' + state.release.version + ' enthält keine Firmware für ' + chip + '.';
    $('blocked').hidden = !problem;
    $('blocked').textContent = problem;
    $('install').hidden = !!problem;
    if (problem) return;
    const m = F.manifest(state.release, chip, location.href, provisionUrl);
    const murl = URL.createObjectURL(new Blob([JSON.stringify(m)], { type: 'application/json' }));
    state.urls.push(murl);
    $('install').setAttribute('manifest', murl);
  }

  function setMode(mode) {
    state.mode = mode;
    if (!mode.startsWith('keep:')) {
      state.hw = H.preset(mode);
      renderSensors();
    }
    $('modeHint').textContent = {
      pcb_v1: 'Das Fabmobil-PCB: zwei Analogkanäle über den Multiplexer, DHT11, Display, LED-Ampel.',
      esp32_devkit: 'Dieselbe Peripherie wie das PCB auf einem ESP32-DevKit.',
    }[mode] || 'Für Geräte, deren Sensoren und Pins schon eingerichtet sind.';
    changed();
  }

  function label(r) {
    return 'v' + r.version + (r.prerelease ? ' (Vorabversion)' : '') + (r.date ? ' – ' + r.date : '');
  }

  function wire() {
    $('mode').addEventListener('change', () => setMode($('mode').value));
    ['deviceName', 'adminPassword', 'wifiSsid', 'wifiPassword', 'ipIp', 'ipGateway', 'ipSubnet', 'ipDns1']
      .forEach(id => $(id).addEventListener('input', changed));
    $('version').addEventListener('change', () => {
      state.release = state.releases[Number($('version').value)];
      $('notes').href = state.release.url || '#';
      $('notes').hidden = !state.release.url;
      changed();
    });
    fetch('releases.json', { cache: 'no-store' }).then(r => r.json()).then(data => {
      state.releases = data.releases || [];
      if (!state.releases.length) {
        $('empty').hidden = false;
        $('version').disabled = true;
      }
      state.releases.forEach((r, i) => $('version').appendChild(el('option', { value: String(i), text: label(r) })));
      const stable = state.releases.findIndex(r => !r.prerelease);
      if (state.releases.length) {
        $('version').value = String(stable >= 0 ? stable : 0);
        state.release = state.releases[Number($('version').value)];
        $('notes').href = state.release.url || '#';
        $('notes').hidden = !state.release.url;
      }
      setMode('pcb_v1');
    }).catch(() => {
      $('empty').hidden = false;
      $('empty').textContent = 'Versionsliste konnte nicht geladen werden.';
      setMode('pcb_v1');
    });
  }

  document.addEventListener('DOMContentLoaded', wire);
})();
