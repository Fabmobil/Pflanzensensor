/**
 * @file hardware.js
 * @brief Hardware-Beschreibung im Webflasher: Voreinstellungen, Prüfregeln
 * @details Dieselben Regeln wie firmware/src/core/hardware_check.h; beide
 *          werden gegen firmware/test/vectors/hardware_check.json getestet.
 *          Voreinstellungen entsprechen firmware/boards/pcb_v1.h und
 *          esp32_devkit.h (Test vergleicht sie).
 */
(function (global) {
  'use strict';

  const PIN_FIELDS = ['adc', 'muxA', 'muxB', 'muxC', 'dht', 'ledGreen', 'ledYellow', 'ledRed', 'sda', 'scl',
    'oneWire', 'sdsRx', 'sdsTx', 'mhz19Pwm', 'hxSck', 'hxDout', 'serialRx', 'serialTx'];

  /** Voreinstellungen der Struktur (wie core/board_types.h) */
  function emptyFeatures() {
    return {
      analog: false, analogChannels: 0, multiplexer: false, dht: false, dhtType: 11, ds18b20: false,
      ds18b20Max: 0, sds011: false, mhz19: false, mhz19Range: 5000, hx711: false, bmp280: false,
      bmp280Address: 0x76, serialReceiver: false, serialBaud: 9600, serialTimeoutMs: 4000, display: false,
      displayAddress: 0x3C, ledTraffic: false
    };
  }
  function emptyPins() {
    const p = {};
    PIN_FIELDS.forEach(k => { p[k] = -1; });
    return p;
  }

  const PRESETS = {
    pcb_v1: {
      label: 'Pflanzensensor-PCB (ESP8266, NodeMCU)', chip: 'ESP8266',
      features: { analog: true, analogChannels: 2, multiplexer: true, dht: true, dhtType: 11, display: true,
        displayAddress: 0x3C, ledTraffic: true },
      pins: { adc: 17, muxA: 15, muxB: 2, muxC: 16, dht: 0, ledGreen: 14, ledYellow: 12, ledRed: 13, sda: 4, scl: 5 }
    },
    esp32_devkit: {
      label: 'ESP32-DevKit (WROOM-32)', chip: 'ESP32',
      features: { analog: true, analogChannels: 2, multiplexer: true, dht: true, dhtType: 11, display: true,
        ledTraffic: true },
      pins: { adc: 34, muxA: 25, muxB: 26, muxC: 27, dht: 4, ledGreen: 16, ledYellow: 17, ledRed: 18, sda: 21, scl: 22 }
    }
  };

  /** Vollständige Hardware aus einer Voreinstellung */
  function preset(name) {
    const p = PRESETS[name];
    return {
      name, chip: p.chip,
      features: Object.assign(emptyFeatures(), p.features),
      pins: Object.assign(emptyPins(), p.pins),
      staticIp: null
    };
  }

  const MAX_CHANNELS = { Analog: 8, Ds18b20: 8 };

  /**
   * Messkanäle je Sensor in Firmware-Reihenfolge (SensorType) mit den
   * Standardnamen aus firmware/src/core/sensors/catalog.cpp (Test vergleicht).
   * Schlüssel wie in der Oberfläche: "<ID>_<Index>".
   */
  const CHANNELS = [
    { id: 'ANALOG', feature: 'analog', names: ['Lichtstärke', 'Bodenfeuchte', 'Bodenfeuchte 2', 'Bodenfeuchte 3',
      'Bodenfeuchte 4', 'Bodenfeuchte 5', 'Bodenfeuchte 6', 'Bodenfeuchte 7'] },
    { id: 'DHT', feature: 'dht', names: ['Lufttemperatur', 'Luftfeuchte'] },
    { id: 'DS18B20', feature: 'ds18b20', names: ['DS18B20_1', 'DS18B20_2', 'DS18B20_3', 'DS18B20_4', 'DS18B20_5',
      'DS18B20_6', 'DS18B20_7', 'DS18B20_8'] },
    { id: 'SDS011', feature: 'sds011', names: ['PM10', 'PM2.5'] },
    { id: 'MHZ19', feature: 'mhz19', names: ['CO2'] },
    { id: 'HX711', feature: 'hx711', names: ['Gewicht'] },
    { id: 'BMP280', feature: 'bmp280', names: ['BMP_Temperatur', 'Luftdruck'] },
    { id: 'SERIAL_RECEIVER', feature: 'serialReceiver', names: ['Flow Rate', 'Absolute Counts', 'Sum Flow Rate',
      '24h Flow Rate', 'Arduino Time', 'Uptime', 'Liters per Hour'] }
  ];

  /** Kanäle der bestückten Sensoren: [{key, name, feature}] (wie board::slot) */
  function channels(f) {
    const out = [];
    CHANNELS.forEach(c => {
      if (!f[c.feature]) return;
      const n = c.feature === 'analog' ? f.analogChannels : c.feature === 'ds18b20' ? f.ds18b20Max : c.names.length;
      for (let i = 0; i < n && i < c.names.length; i++) out.push({ key: c.id + '_' + i, name: c.names[i], feature: c.feature });
    });
    return out;
  }

  /** Messkanäle zusammen (wie board::channelsOf) */
  function channelCount(f) {
    return (f.analog ? f.analogChannels : 0) + (f.dht ? 2 : 0) + (f.ds18b20 ? f.ds18b20Max : 0) +
      (f.sds011 ? 2 : 0) + (f.mhz19 ? 1 : 0) + (f.hx711 ? 1 : 0) + (f.bmp280 ? 2 : 0) + (f.serialReceiver ? 7 : 0);
  }

  function maxChannels(chip) { return chip === 'ESP8266' ? 16 : 31; }

  /** Pinbelegung in derselben Reihenfolge wie hwcheck::uses() */
  function uses(f, p) {
    const mux = f.analog && f.multiplexer, i2c = f.display || f.bmp280;
    return [
      { pin: p.adc, used: f.analog, pullup: false, output: false, adc: true, what: 'Analog-Eingang' },
      { pin: p.muxA, used: mux, pullup: false, output: true, adc: false, what: 'Multiplexer A' },
      { pin: p.muxB, used: mux, pullup: false, output: true, adc: false, what: 'Multiplexer B' },
      { pin: p.muxC, used: mux, pullup: false, output: true, adc: false, what: 'Multiplexer C' },
      { pin: p.dht, used: f.dht, pullup: true, output: true, adc: false, what: 'DHT' },
      { pin: p.ledGreen, used: f.ledTraffic, pullup: false, output: true, adc: false, what: 'LED grün' },
      { pin: p.ledYellow, used: f.ledTraffic, pullup: false, output: true, adc: false, what: 'LED gelb' },
      { pin: p.ledRed, used: f.ledTraffic, pullup: false, output: true, adc: false, what: 'LED rot' },
      { pin: p.sda, used: i2c, pullup: true, output: true, adc: false, what: 'I²C SDA' },
      { pin: p.scl, used: i2c, pullup: true, output: true, adc: false, what: 'I²C SCL' },
      { pin: p.oneWire, used: f.ds18b20, pullup: true, output: true, adc: false, what: 'OneWire (DS18B20)' },
      { pin: p.sdsRx, used: f.sds011, pullup: true, output: false, adc: false, what: 'SDS011 RX' },
      { pin: p.sdsTx, used: f.sds011, pullup: false, output: true, adc: false, what: 'SDS011 TX' },
      { pin: p.mhz19Pwm, used: f.mhz19, pullup: true, output: false, adc: false, what: 'MH-Z19 PWM' },
      { pin: p.hxSck, used: f.hx711, pullup: false, output: true, adc: false, what: 'HX711 SCK' },
      { pin: p.hxDout, used: f.hx711, pullup: true, output: false, adc: false, what: 'HX711 DOUT' },
      { pin: p.serialRx, used: f.serialReceiver, pullup: true, output: false, adc: false, what: 'Serial RX' },
      { pin: p.serialTx, used: f.serialReceiver, pullup: false, output: true, adc: false, what: 'Serial TX' }
    ];
  }

  function exists(chip, pin) {
    if (chip === 'ESP8266') return (pin >= 0 && pin <= 16) || pin === 17;
    if (chip === 'ESP32') return pin >= 0 && pin <= 39 && pin !== 20 && pin !== 24 && !(pin >= 28 && pin <= 31);
    return pin >= 0;
  }
  function adcCapable(chip, pin) {
    if (chip === 'ESP8266') return pin === 17;
    if (chip === 'ESP32') return pin >= 32 && pin <= 39;
    return true;
  }

  /**
   * Prüfen wie checkHardware() in der Firmware.
   * @return {problem, pin, what}  problem 'ok' wenn alles passt
   */
  function check(chip, f, p) {
    const res = (problem, pin, what) => ({ problem, pin: pin === undefined ? -1 : pin, what: what || '' });
    if (!f.analog && !f.dht && !f.ds18b20 && !f.sds011 && !f.mhz19 && !f.hx711 && !f.bmp280 && !f.serialReceiver)
      return res('no_sensor');
    if (f.analog && (f.analogChannels < 1 || f.analogChannels > 8)) return res('analog_channels');
    if (f.analog && f.analogChannels > 1 && !f.multiplexer) return res('need_multiplexer');
    if (f.ds18b20 && (f.ds18b20Max < 1 || f.ds18b20Max > 8)) return res('ds18b20_count');
    if (f.mhz19 && f.mhz19Range !== 2000 && f.mhz19Range !== 5000) return res('mhz19_range');
    if (channelCount(f) > maxChannels(chip)) return res('too_many_channels');
    const u = uses(f, p);
    for (let i = 0; i < u.length; i++) {
      if (!u[i].used) continue;
      const pin = u[i].pin;
      if (pin < 0) return res('missing_pin', -1, u[i].what);
      if (!exists(chip, pin)) return res('no_such_pin', pin, u[i].what);
      if (chip !== 'native' && pin >= 6 && pin <= 11) return res('flash_pin', pin, u[i].what);
      if (u[i].adc && !adcCapable(chip, pin)) return res('adc_pin', pin, u[i].what);
      if (chip === 'ESP8266' && !u[i].adc && pin === 17) return res('adc_pin', pin, u[i].what);
      if (chip === 'ESP8266' && pin === 15 && u[i].pullup) return res('strap_pin', pin, u[i].what);
      if (chip === 'ESP32' && pin >= 34 && u[i].output) return res('input_only_pin', pin, u[i].what);
      for (let j = i + 1; j < u.length; j++)
        if (u[j].used && u[j].pin === pin) return res('duplicate_pin', pin, u[i].what + ' / ' + u[j].what);
    }
    return res('ok');
  }

  const PROBLEM_TEXT = {
    ok: 'Passt.',
    no_sensor: 'Mindestens ein Sensor muss bestückt sein.',
    analog_channels: 'Analogkanäle: 1 bis 8.',
    need_multiplexer: 'Mehr als ein Analogkanal braucht den Multiplexer.',
    ds18b20_count: 'DS18B20: 1 bis 8 Fühler.',
    mhz19_range: 'MH-Z19: Messbereich 2000 oder 5000 ppm.',
    too_many_channels: 'Zu viele Messkanäle für diesen Chip (ESP8266 höchstens 16).',
    missing_pin: 'Pin fehlt',
    no_such_pin: 'Diesen Pin gibt es auf dem Chip nicht',
    flash_pin: 'GPIO 6–11 gehören zum Flash-Speicher',
    adc_pin: 'Dieser Pin kann nicht analog messen (ESP8266: nur A0 = 17; ESP32: 32–39)',
    strap_pin: 'GPIO 15 muss beim Start LOW sein – nicht für Eingänge mit Pull-up',
    input_only_pin: 'GPIO 34–39 sind reine Eingänge',
    duplicate_pin: 'Pin doppelt belegt'
  };

  function describe(r) {
    const base = PROBLEM_TEXT[r.problem] || r.problem;
    if (r.problem === 'ok') return base;
    return base + (r.what ? ': ' + r.what : '') + (r.pin >= 0 ? ' (GPIO ' + r.pin + ')' : '') + '.';
  }

  global.Hardware = { PIN_FIELDS, PRESETS, emptyFeatures, emptyPins, preset, channelCount, maxChannels, uses,
    check, describe, MAX_CHANNELS, CHANNELS, channels };
})(typeof window !== 'undefined' ? window : globalThis);
