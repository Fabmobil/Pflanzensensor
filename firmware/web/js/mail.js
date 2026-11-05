/**
 * @file mail.js
 * @brief Seite E-Mail: Einstellungen, Schlüssel, Kanalauswahl, Testmail, Status
 */
(function (global) {
  'use strict';
  const { el, toast } = global.UI;
  const FIELDS = ['enabled', 'url', 'deviceId', 'to', 'warnFrom', 'warnHours', 'boot', 'alive', 'aliveHours'];
  let polling = null;

  function readForm() {
    const mail = {};
    FIELDS.forEach(f => {
      const e = document.getElementById('mail-' + f);
      if (!e) return;
      mail[f] = e.type === 'checkbox' ? e.checked : e.type === 'number' ? Number(e.value) : e.value;
    });
    const watched = Array.from(document.querySelectorAll('#mail-watch input:checked')).map(c => c.value);
    const all = document.querySelectorAll('#mail-watch input').length;
    mail.watch = watched.length === all ? '' : watched.join(',');
    return mail;
  }

  /** Auswahl: leer = alle überwacht */
  function isWatched(selection, key) {
    if (!selection) return true;
    return selection.split(',').map(s => s.trim()).includes(key);
  }

  function save() {
    const key = document.getElementById('mail-key').value.trim();
    const steps = [global.Api.patch('/api/v1/config', { mail: readForm() })];
    if (key) steps.push(global.Api.put('/api/v1/secrets/mail-key', { key }).then(() => {
      document.getElementById('mail-key').value = '';
      document.getElementById('mail-key').placeholder = '(gespeichert)';
    }));
    return Promise.all(steps).then(() => toast('Gespeichert'));
  }

  function test() {
    return global.Api.post('/api/v1/mail/test').then(() => {
      toast('Testmail angefordert');
      poll();
    });
  }

  function showStatus(s) {
    const box = document.getElementById('mail-status');
    const when = s.lastAttempt ? new Date(s.lastAttempt * 1000).toLocaleString('de-DE') : 'noch nie';
    box.textContent = `Letzter Versand: ${when} – ${s.result || '–'} (${s.sent} erfolgreich)`;
    if (s.pending && !polling) polling = setInterval(poll, 3000);
    if (!s.pending && polling) {
      clearInterval(polling);
      polling = null;
    }
  }

  function poll() {
    return global.Api.get('/api/v1/mail/status', { quiet: true }).then(showStatus).catch(() => {});
  }

  function wire() {
    global.Api.get('/api/v1/config').then(c => {
      const m = c.config.mail;
      FIELDS.forEach(f => {
        const e = document.getElementById('mail-' + f);
        if (!e) return;
        if (e.type === 'checkbox') e.checked = !!m[f];
        else e.value = m[f];
      });
      document.getElementById('mail-key').placeholder = c.secrets.mailKey.set ? '(gespeichert - leer lassen = behalten)' : '(noch kein Schlüssel)';
      const box = document.getElementById('mail-watch');
      c.channels.filter(ch => ch.enabled).forEach(ch => {
        box.appendChild(el('label', {}, [el('input', { type: 'checkbox', value: ch.key, checked: isWatched(m.watch, ch.key) }), ' ' + ch.name]));
      });
      const toggle = () => document.getElementById('mail-fields').classList.toggle('aus', !document.getElementById('mail-enabled').checked);
      document.getElementById('mail-enabled').addEventListener('change', toggle);
      toggle();
    });
    document.getElementById('mail-save').addEventListener('click', save);
    document.getElementById('mail-save-test').addEventListener('click', () => save().then(test));
    document.getElementById('mail-test').addEventListener('click', test);
    poll();
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Mail = { isWatched, readForm };
})(typeof window !== 'undefined' ? window : globalThis);
