/**
 * @file configimport.js
 * @brief Konfigurationsdatei (config.json der Firmware 2.x oder Export der Firmware 3) übernehmen
 * @details Was im Formular steht (Gerätename, Adminpasswort, WLAN, Kanalnamen),
 *          kommt sichtbar ins Formular und wird aus der Datei entfernt – so gilt,
 *          was dort steht. Den Rest (Grenzwerte, Display, LED-Ampel, Log, Mail,
 *          Mailtexte …) schreibt die Seite als "settings.config" in den Block;
 *          das Gerät übernimmt ihn beim ersten Start mit seinem Import
 *          (ConfigService::importJson, für 2.x legacy::import). Die Formate
 *          kennt also nur die Firmware; hier wird nur erkannt, verteilt und gekürzt.
 */
(function (global) {
  'use strict';

  /** Sensor-ID → Schalter in Hardware.features */
  const FEATURE = { ANALOG: 'analog', DHT: 'dht', DS18B20: 'ds18b20', SDS011: 'sds011', MHZ19: 'mhz19',
    HX711: 'hx711', BMP280: 'bmp280', SERIAL_RECEIVER: 'serialReceiver' };

  const isObj = v => !!v && typeof v === 'object' && !Array.isArray(v);
  const str = v => (typeof v === 'string' ? v : '');
  const clone = v => JSON.parse(JSON.stringify(v));

  /** 'v2' | 'v3' | null – wie legacy::isLegacy() und der Schema-Check der Firmware */
  function detect(doc) {
    if (!isObj(doc)) return null;
    if (doc.schema === undefined) return isObj(doc.general) || doc.sensors !== undefined ? 'v2' : null;
    return doc.schema === 1 ? 'v3' : null;
  }

  /** Sensor-ID aus einem Kanalschlüssel ("MHZ19_0" → "MHZ19") */
  function sensorOfKey(key) {
    const m = /^([A-Z0-9_]+)_\d+$/.exec(str(key));
    return m ? m[1] : '';
  }

  function fromV2(doc) {
    const config = clone(doc);
    const g = isObj(config.general) ? config.general : {};
    const form = { deviceName: str(g.device_name).trim(), adminPassword: str(g.admin_pwd), wifi: [null, null, null], channelNames: {} };
    delete g.device_name;
    delete g.admin_pwd;
    const w = isObj(config.wifi) ? config.wifi : {};
    for (let i = 0; i < 3; i++) {
      const ssid = str(w['ssid' + (i + 1)]).trim();
      if (ssid) form.wifi[i] = { ssid, password: str(w['pwd' + (i + 1)]) };
    }
    delete config.wifi;
    const used = new Set();
    (Array.isArray(config.sensors) ? config.sensors : []).forEach(grp => {
      if (!isObj(grp) || !str(grp.id)) return;
      used.add(grp.id);
      // Neuere 2.x: Messungen als Liste; ältere: Gruppe = eine Messung
      const list = Array.isArray(grp.measurements) ? grp.measurements : [grp];
      list.forEach(m => {
        if (!isObj(m) || !str(m.nm).trim()) return;
        form.channelNames[grp.id + '_' + (Number.isInteger(m.idx) ? m.idx : 0)] = m.nm.trim();
        delete m.nm;
      });
    });
    [g.flower_sens, isObj(config.led_traffic) ? config.led_traffic.sel_meas : ''].forEach(k => {
      if (sensorOfKey(k)) used.add(sensorOfKey(k));
    });
    return { format: 'v2', form, config, sensors: [...used], wifiNames: [] };
  }

  function fromV3(doc) {
    const config = clone(doc);
    const dev = isObj(config.device) ? config.device : {};
    const g = isObj(dev.general) ? dev.general : {};
    const form = { deviceName: str(g.deviceName).trim(), adminPassword: '', wifi: [null, null, null], channelNames: {} };
    delete g.deviceName;
    // Exporte der Firmware 3 enthalten keine Passwörter. Nur die SSID zu setzen,
    // hieße "offenes Netz" – deshalb bleibt das WLAN hier ganz draußen.
    const wifiNames = [1, 2, 3].map(i => str(isObj(dev.wifi) ? dev.wifi['ssid' + i] : '').trim()).filter(Boolean);
    delete dev.wifi;
    const used = new Set();
    const sensors = isObj(config.sensors) ? config.sensors : {};
    Object.entries(sensors).forEach(([id, s]) => {
      used.add(id);
      if (!isObj(s)) return;
      // Felder ohne Bedeutung für diesen Sensor weglassen (Platz im 4-KB-Block)
      if (id !== 'HX711') {
        delete s.scale;
        delete s.tare;
      }
      Object.entries(isObj(s.channels) ? s.channels : {}).forEach(([idx, ch]) => {
        if (!isObj(ch)) return;
        if (str(ch.name).trim()) form.channelNames[id + '_' + idx] = ch.name.trim();
        delete ch.name;
        if (id !== 'ANALOG') ['rawMin', 'rawMax', 'inverted', 'autocal', 'halfLife'].forEach(k => delete ch[k]);
      });
    });
    [g.flowerChannel, isObj(dev.led) ? dev.led.channel : ''].forEach(k => {
      if (sensorOfKey(k)) used.add(sensorOfKey(k));
    });
    if (isObj(config.templates) && !Object.keys(config.templates).length) delete config.templates;
    return { format: 'v3', form, config, sensors: [...used], wifiNames };
  }

  /**
   * Datei lesen. Wirft Error mit deutscher Meldung.
   * @returns {format, form: {deviceName, adminPassword, wifi[3], channelNames}, config, sensors, wifiNames}
   */
  function read(text) {
    let doc;
    try {
      doc = JSON.parse(text);
    } catch (e) {
      throw new Error('Die Datei ist kein gültiges JSON.');
    }
    const format = detect(doc);
    if (format === 'v2') return fromV2(doc);
    if (format === 'v3') return fromV3(doc);
    if (isObj(doc) && doc.schema !== undefined) throw new Error('Unbekannte Version der Konfiguration (schema ' + doc.schema + ').');
    throw new Error('Das ist keine Konfigurationsdatei des Pflanzensensors.');
  }

  /** Sensoren aus der Datei, die in der Hardware-Angabe fehlen (deren Werte ignoriert das Gerät) */
  function missingSensors(imported, features) {
    return imported.sensors.filter(id => FEATURE[id] && !features[FEATURE[id]]);
  }

  /**
   * Block erzeugen; passt er nicht in 4 KB, zuerst ohne Mailtexte versuchen.
   * @returns {bytes, droppedTemplates} – bytes null, wenn es auch so nicht passt
   */
  function encodeFitting(Provision, hw, settings) {
    try {
      return { bytes: Provision.encode(hw, settings), droppedTemplates: false };
    } catch (e) {
      if (!settings.config || !settings.config.templates) return { bytes: null, droppedTemplates: false };
    }
    const smaller = Object.assign({}, settings, { config: Object.assign({}, settings.config) });
    delete smaller.config.templates;
    try {
      return { bytes: Provision.encode(hw, smaller), droppedTemplates: true };
    } catch (e) {
      return { bytes: null, droppedTemplates: true };
    }
  }

  global.ConfigImport = { FEATURE, detect, read, missingSensors, encodeFitting, sensorOfKey };
})(typeof window !== 'undefined' ? window : globalThis);
