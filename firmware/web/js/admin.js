/**
 * @file admin.js
 * @brief Seite Einstellungen: System, Aktionen, WLAN, Systeminfo, Logging
 */
(function (global) {
  'use strict';
  const { el, toast } = global.UI;

  /** Hostname wie MdnsName::hostnameFrom (für die Vorschau) */
  function hostnameFrom(name) {
    const map = { 'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'ae', 'Ö': 'oe', 'Ü': 'ue', 'ß': 'ss' };
    let out = '';
    for (const ch of name || '') {
      let c = map[ch];
      if (c === undefined) {
        const l = ch.toLowerCase();
        c = /^[a-z0-9-]$/.test(l) && ch.charCodeAt(0) < 128 ? l : '-';
      }
      for (const x of c) {
        if (x === '-' && (out === '' || out.endsWith('-'))) continue;
        if (out.length < 63) out += x;
      }
    }
    out = out.replace(/-+$/, '');
    return out || 'pflanzensensor';
  }

  function passwordProblem(a, b) {
    if (!a) return 'Bitte ein Passwort eingeben.';
    if (a !== b) return 'Die Passwörter stimmen nicht überein.';
    if (a.length > 32) return 'Höchstens 32 Zeichen.';
    if (!/^[\x20-\x7e]+$/.test(a)) return 'Nur druckbare ASCII-Zeichen (keine Umlaute).';
    return null;
  }

  function reboot(title, trigger, timeoutMs) {
    return global.DeviceWait.runReboot({ title, trigger, timeoutMs });
  }

  function renderWifi(data) {
    const box = document.getElementById('wifiSlots');
    box.textContent = '';
    for (let i = 0; i < 3; i++) {
      const active = data.activeWifiSlot === i;
      const ssid = el('input', { type: 'text', maxlength: '32', value: data.config.wifi['ssid' + (i + 1)] || '', disabled: active });
      const pw = el('input', { type: 'password', maxlength: '64', placeholder: data.secrets.wifi[i].set ? '(gespeichert)' : '(leer)', disabled: active, autocomplete: 'new-password' });
      const clear = el('input', { type: 'checkbox', disabled: active });
      const save = el('button', { class: 'button button-primary', text: 'Speichern', disabled: active });
      save.addEventListener('click', () => {
        global.Api.put('/api/v1/secrets/wifi/' + (i + 1), {
          ssid: ssid.value, password: pw.value, clear: clear.checked,
          reconnect: document.getElementById('wifiReconnect').checked
        }).then(() => { pw.value = ''; toast('WLAN ' + (i + 1) + ' gespeichert'); });
      });
      box.appendChild(el('div', { class: 'card-section wifi-slot' }, [
        el('h3', { text: 'WLAN ' + (i + 1) + (active ? ' (aktiv)' : '') }),
        el('div', { class: 'wifi-fields' }, [
          el('div', { class: 'form-group' }, [el('label', { text: 'SSID' }), ssid]),
          el('div', { class: 'form-group' }, [el('label', { text: 'Passwort' }), pw])]),
        el('div', { class: 'wifi-actions' }, [
          el('label', {}, [clear, ' Passwort löschen (offenes Netz)']), save])
      ]));
    }
  }

  function renderInfo(s) {
    const t = document.getElementById('sysinfo');
    const rows = [
      ['Freier Heap', global.UI.formatBytes(s.freeHeap)],
      ['Minimum seit Start', global.UI.formatBytes(s.minFreeHeap)],
      ['Größter Block', global.UI.formatBytes(s.maxBlock)],
      ['Fragmentierung', s.heapFragmentation + ' %'],
      ['Laufzeit', global.UI.formatUptime(s.uptime)],
      ['SSID', s.ssid || '–'], ['RSSI', s.rssi + ' dBm'], ['IP', s.ip], ['MAC', s.mac],
      ['Dateisystem', global.UI.formatBytes(s.fsUsed) + ' von ' + global.UI.formatBytes(s.fsTotal) +
        ' belegt, ' + global.UI.formatBytes(s.fsTotal - s.fsUsed) + ' frei'],
      ['Log-Datei', global.UI.formatBytes(s.logFileSize) + ' (' + Math.round(100 * s.logFileSize / s.logFileMax) + ' % von max.)'],
      ['Version', s.version + ' (' + s.buildDate + ')']
    ];
    t.textContent = '';
    rows.forEach(([k, v]) => t.appendChild(el('tr', {}, [el('td', { text: k + ':' }), el('td', { text: v })])));
  }

  function wire() {
    const name = document.getElementById('deviceName');
    const preview = () => { document.getElementById('hostPreview').textContent = hostnameFrom(name.value); };
    name.addEventListener('input', preview);
    global.ConfigBind.bindConfig(document, path => {
      if (path === 'general.deviceName') global.Footer.refresh();
    }).then(data => {
      preview();
      renderWifi(data);
      renderInfo(data.system);
    });

    document.getElementById('pwSave').addEventListener('click', () => {
      const a = document.getElementById('pw1').value, b = document.getElementById('pw2').value;
      const problem = passwordProblem(a, b);
      if (problem) return toast(problem, 'error');
      global.Api.put('/api/v1/secrets/admin-password', { password: a }).then(() => {
        toast('Passwort geändert - der Browser fragt gleich neu nach der Anmeldung.');
        document.getElementById('pw1').value = document.getElementById('pw2').value = '';
      });
    });
    document.getElementById('reboot').addEventListener('click', () => {
      global.UI.confirmDialog('Gerät jetzt neu starten?').then(ok => {
        if (ok) reboot('Gerät startet neu …', () => global.Api.post('/api/v1/system/reboot'), 90000);
      });
    });
    document.getElementById('reset').addEventListener('click', () => {
      global.UI.confirmDialog('Wirklich ALLE Einstellungen löschen? Das lässt sich nicht rückgängig machen.').then(ok => {
        if (ok) reboot('Werksreset …', () => global.Api.post('/api/v1/system/factory-reset'), 120000);
      });
    });
    document.getElementById('importBtn').addEventListener('click', () => {
      const f = document.getElementById('importFile').files[0];
      if (!f) return toast('Bitte eine Datei wählen.', 'error');
      const form = new FormData();
      form.append('config', f);
      reboot('Konfiguration wird übernommen …', () =>
        fetch('/api/v1/config/import', { method: 'POST', body: form }).then(r => r.json()).then(r => {
          if (!r.ok) throw new Error(r.message || 'Import fehlgeschlagen');
          toast(`Übernommen: ${r.applied}, ignoriert: ${r.ignored}, ungültig: ${r.invalid}`);
        }), 90000);
    });
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Admin = { hostnameFrom, passwordProblem };
})(typeof window !== 'undefined' ? window : globalThis);
