/**
 * @file flasher.js
 * @brief Manifest für ESP Web Tools aus Release, Chip und Provisionierungsblock
 */
(function (global) {
  'use strict';

  /**
   * @param release Eintrag aus releases.json ({version, parts: {ESP8266: [{path, offset}], …}})
   * @param chip 'ESP8266' | 'ESP32'
   * @param base URL der Seite (Teile werden absolut, weil das Manifest eine blob:-URL ist)
   * @param provisionUrl blob:-URL des 4-KB-Blocks oder null (nur Firmware)
   */
  function manifest(release, chip, base, provisionUrl) {
    const parts = (release.parts[chip] || []).map(p => ({ path: new URL(p.path, base).href, offset: p.offset }));
    if (!parts.length) throw new Error('Keine Firmware für ' + chip + ' in v' + release.version);
    if (provisionUrl) parts.push({ path: provisionUrl, offset: global.Provision.OFFSET[chip] });
    return {
      name: 'Pflanzensensor',
      version: release.version,
      // Nichts löschen, solange man es nicht ankreuzt: Einstellungen, Chronik
      // und Log liegen hinter der Firmware und bleiben stehen.
      new_install_prompt_erase: true,
      // Nach dem Flashen auf Improv warten (WLAN über USB)
      new_install_improv_wait_time: 20,
      builds: [{ chipFamily: chip, parts }]
    };
  }

  /** Einstellungen säubern: nur ausgefüllte Felder */
  function cleanSettings(s) {
    const out = {};
    const name = (s.deviceName || '').trim();
    if (name) out.deviceName = name.slice(0, 32);
    const ssid = (s.wifiSsid || '').trim();
    if (ssid) { out.wifiSsid = ssid.slice(0, 32); out.wifiPassword = (s.wifiPassword || '').slice(0, 64); }
    if (s.adminPassword) out.adminPassword = s.adminPassword;
    return out;
  }

  /** Probleme der Einstellungen (wie die Firmware: Passwort 1..32 druckbare ASCII) */
  function settingsProblem(s) {
    if (s.adminPassword && !/^[\x20-\x7e]{1,32}$/.test(s.adminPassword))
      return 'Adminpasswort: 1 bis 32 druckbare ASCII-Zeichen.';
    if (s.wifiPassword && s.wifiPassword.length > 0 && s.wifiPassword.length < 8)
      return 'WLAN-Passwort: mindestens 8 Zeichen (oder leer für ein offenes Netz).';
    const ip = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
    for (const k of ['ip', 'gateway', 'subnet', 'dns1']) {
      const v = (s.staticIp && s.staticIp[k] || '').trim();
      if (v && (!ip.test(v) || v.split('.').some(x => Number(x) > 255))) return 'Feste IP: „' + v + '“ ist keine IPv4-Adresse.';
    }
    if (s.staticIp && (s.staticIp.ip || '').trim() && !(s.staticIp.gateway || '').trim())
      return 'Feste IP: Gateway fehlt.';
    return '';
  }

  global.Flasher = { manifest, cleanSettings, settingsProblem };
})(typeof window !== 'undefined' ? window : globalThis);
