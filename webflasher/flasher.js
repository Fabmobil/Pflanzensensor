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

  /** Slot 3 ab Werk (Firmware: defaults::kFabmobilWifiSsid, Passwort steckt nur in der Firmware) */
  const FABMOBIL_SSID = 'Fabmobil Gäste';

  /** Länge in UTF-8-Bytes (die Firmware zählt Bytes) */
  function utf8Length(s) { return new TextEncoder().encode(s).length; }

  /**
   * Einstellungen säubern: nur ausgefüllte Felder.
   * s.wifi: [{ssid, password} ×3]. Slot 3 unverändert auf dem Fabmobil-WLAN ohne
   * Passwort wird nicht geschickt – die Firmware bringt es als Voreinstellung mit.
   * s.channelNames: {key: name}; leere Namen bleiben weg (Gerät behält seinen).
   */
  function cleanSettings(s) {
    const out = {};
    const name = (s.deviceName || '').trim();
    if (name) out.deviceName = name.slice(0, 32);
    const wifi = [0, 1, 2].map(i => {
      const w = (s.wifi || [])[i] || {};
      const ssid = (w.ssid || '').trim();
      if (!ssid) return null;
      if (i === 2 && ssid === FABMOBIL_SSID && !w.password) return null;
      return { ssid: ssid.slice(0, 32), password: (w.password || '').slice(0, 64) };
    });
    if (wifi.some(w => w)) out.wifi = wifi;
    if (s.adminPassword) out.adminPassword = s.adminPassword;
    const names = {};
    Object.entries(s.channelNames || {}).forEach(([k, v]) => {
      const n = (v || '').trim();
      if (n) names[k] = n;
    });
    if (Object.keys(names).length) out.channelNames = names;
    return out;
  }

  /** Probleme der Einstellungen (wie die Firmware: Passwort 1..32 druckbare ASCII) */
  function settingsProblem(s) {
    if (s.adminPassword && !/^[\x20-\x7e]{1,32}$/.test(s.adminPassword))
      return 'Adminpasswort: 1 bis 32 druckbare ASCII-Zeichen.';
    if (utf8Length((s.deviceName || '').trim()) > 32) return 'Gerätename: höchstens 32 Bytes (Umlaute zählen doppelt).';
    for (let i = 0; i < 3; i++) {
      const w = (s.wifi || [])[i] || {};
      if (w.password && w.password.length < 8)
        return 'WLAN ' + (i + 1) + ': Passwort mindestens 8 Zeichen (oder leer für ein offenes Netz).';
      if (w.password && !(w.ssid || '').trim()) return 'WLAN ' + (i + 1) + ': Name (SSID) fehlt.';
      if (utf8Length((w.ssid || '').trim()) > 32) return 'WLAN ' + (i + 1) + ': Name höchstens 32 Bytes.';
    }
    for (const [k, v] of Object.entries(s.channelNames || {}))
      if (utf8Length((v || '').trim()) > 24) return 'Kanalname „' + v.trim() + '“ (' + k + '): höchstens 24 Bytes (Umlaute zählen doppelt).';
    const ip = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
    for (const k of ['ip', 'gateway', 'subnet', 'dns1']) {
      const v = (s.staticIp && s.staticIp[k] || '').trim();
      if (v && (!ip.test(v) || v.split('.').some(x => Number(x) > 255))) return 'Feste IP: „' + v + '“ ist keine IPv4-Adresse.';
    }
    if (s.staticIp && (s.staticIp.ip || '').trim() && !(s.staticIp.gateway || '').trim())
      return 'Feste IP: Gateway fehlt.';
    return '';
  }

  global.Flasher = { manifest, cleanSettings, settingsProblem, FABMOBIL_SSID };
})(typeof window !== 'undefined' ? window : globalThis);
