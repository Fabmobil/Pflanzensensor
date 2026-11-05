/**
 * @file provision.js
 * @brief Provisionierungsblock (4 KB) für die Firmware erzeugen
 * @details Format wie firmware/src/core/provision.h:
 *          "PSPROV1\0" | Länge (u32 LE) | CRC-32 (u32 LE) | JSON | 0xFF…
 */
(function (global) {
  'use strict';

  const SECTOR = 4096;
  const HEADER = 16;
  const MAGIC = [0x50, 0x53, 0x50, 0x52, 0x4F, 0x56, 0x31, 0x00]; // "PSPROV1\0"
  /** Adresse des Sektors je Chip (ESP8266: hinter LittleFS, ESP32: Partition "provision") */
  const OFFSET = { ESP8266: 0x3FA000, ESP32: 0x3F0000 };

  let table = null;
  function crc32(bytes) {
    if (!table) {
      table = new Uint32Array(256);
      for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        table[n] = c >>> 0;
      }
    }
    let c = 0xFFFFFFFF;
    for (let i = 0; i < bytes.length; i++) c = table[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  }

  function ipString(v) { return typeof v === 'string' ? v.trim() : ''; }

  /**
   * JSON für den Block. settings (wie Flasher.cleanSettings): {deviceName, adminPassword,
   * wifi: [{ssid, password} | null ×3], channelNames: {"ANALOG_0": "…"}, config: {…}} – leere Felder fehlen.
   * config: importierte Konfigurationsdatei (ConfigImport.read), übernimmt das Gerät vor den übrigen Feldern.
   */
  function blockJson(hw, settings) {
    const doc = {
      hardware: { name: hw.name, features: Object.assign({}, hw.features), pins: Object.assign({}, hw.pins) }
    };
    if (hw.staticIp && ipString(hw.staticIp.ip)) {
      doc.hardware.staticIp = {};
      ['ip', 'gateway', 'subnet', 'dns1', 'dns2'].forEach(k => {
        if (ipString(hw.staticIp[k])) doc.hardware.staticIp[k] = ipString(hw.staticIp[k]);
      });
    }
    const s = {};
    if (settings) {
      if (settings.deviceName) s.deviceName = settings.deviceName;
      if (settings.adminPassword) s.adminPassword = settings.adminPassword;
      const wifi = (settings.wifi || []).slice(0, 3).map(w => (w && w.ssid ? { ssid: w.ssid, password: w.password || '' } : null));
      if (wifi.some(w => w)) s.wifi = wifi;
      const names = settings.channelNames || {};
      if (Object.keys(names).length) s.channelNames = Object.assign({}, names);
      if (settings.config && typeof settings.config === 'object') s.config = settings.config;
    }
    if (Object.keys(s).length) doc.settings = s;
    return JSON.stringify(doc);
  }

  /** Block als Uint8Array(4096) */
  function encode(hw, settings) {
    const json = new TextEncoder().encode(blockJson(hw, settings));
    if (json.length > SECTOR - HEADER) throw new Error('Block zu groß (' + json.length + ' von ' + (SECTOR - HEADER) + ' Bytes)');
    const out = new Uint8Array(SECTOR).fill(0xFF);
    out.set(MAGIC, 0);
    const dv = new DataView(out.buffer);
    dv.setUint32(8, json.length, true);
    dv.setUint32(12, crc32(json), true);
    out.set(json, HEADER);
    return out;
  }

  global.Provision = { SECTOR, OFFSET, crc32, blockJson, encode };
})(typeof window !== 'undefined' ? window : globalThis);
