/**
 * @file update.js
 * @brief Firmware-Update: Upload mit echtem Fortschritt (XHR upload.onprogress),
 *        danach Warten auf den Neustart (DeviceWait)
 */
(function (global) {
  'use strict';
  const { toast } = global.UI;

  function uploadUrl(file, md5) {
    let url = '/api/v1/ota?size=' + file.size;
    if (md5) url += '&md5=' + encodeURIComponent(md5);
    return url;
  }

  function validMd5(md5) { return /^[0-9a-fA-F]{32}$/.test(md5); }

  function setProgress(p, text) {
    document.getElementById('otaBar').style.width = p + '%';
    document.getElementById('otaStatus').textContent = text;
  }

  function start() {
    const file = document.getElementById('otaFile').files[0];
    const md5 = document.getElementById('otaMd5').value.trim();
    if (!file) return toast('Bitte eine firmware.bin wählen.', 'error');
    if (!/\.bin$/i.test(file.name)) return toast('Nur .bin-Dateien.', 'error');
    if (md5 && !validMd5(md5)) return toast('Die Prüfsumme muss 32 Hex-Zeichen haben.', 'error');
    if (document.getElementById('md5Group').dataset.required === '1' && !md5)
      return toast('Prüfsumme (MD5) wird verlangt.', 'error');
    global.DeviceWait.getStatus().then(baseline => {
      const form = new FormData();
      form.append('firmware', file, file.name);
      const xhr = new XMLHttpRequest();
      xhr.open('POST', uploadUrl(file, md5));
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) setProgress(Math.round(e.loaded * 100 / e.total), `Übertrage … ${Math.round(e.loaded / 1024)} KB`);
      };
      xhr.onload = () => {
        let r = {};
        try { r = JSON.parse(xhr.responseText); } catch (e) { r = {}; }
        if (xhr.status === 200 && r.success) {
          setProgress(100, 'Übertragen. Gerät startet neu …');
          global.DeviceWait.runReboot({ title: 'Firmware wird aktiviert …', timeoutMs: 120000,
                                        trigger: () => Promise.resolve(), until: s => s.bootId !== baseline.bootId });
        } else {
          setProgress(0, 'Fehlgeschlagen: ' + (r.message || ('HTTP ' + xhr.status)) + ' - die alte Firmware bleibt aktiv.');
        }
      };
      xhr.onerror = () => setProgress(0, 'Verbindung abgebrochen - die alte Firmware bleibt aktiv.');
      setProgress(0, 'Starte Upload …');
      xhr.send(form);
    });
  }

  function wire() {
    global.Api.get('/api/v1/config').then(c => {
      document.getElementById('otaVersion').textContent = c.system.version + ' (' + c.system.buildDate + ')';
      if (c.config.general.otaRequireMd5) {
        const g = document.getElementById('md5Group');
        g.dataset.required = '1';
        g.querySelector('label').textContent = 'Prüfsumme (MD5, verlangt)';
      }
    });
    document.getElementById('otaStart').addEventListener('click', start);
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Update = { uploadUrl, validMd5 };
})(typeof window !== 'undefined' ? window : globalThis);
