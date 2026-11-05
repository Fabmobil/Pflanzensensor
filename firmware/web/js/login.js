/**
 * @file login.js
 * @brief Anmeldung nur mit Passwort; danach zurück zur aufgerufenen Seite
 */
(function (global) {
  'use strict';

  /** Rücksprungziel aus ?next=, nur Pfade auf diesem Gerät */
  function nextTarget(search) {
    const m = /[?&]next=([^&]*)/.exec(search || '');
    const p = m ? decodeURIComponent(m[1]) : '/admin';
    return /^\/(?!\/)/.test(p) && p !== '/login' ? p : '/admin';
  }

  function wire() {
    const form = document.getElementById('loginForm');
    const msg = document.getElementById('loginMsg');
    form.addEventListener('submit', e => {
      e.preventDefault();
      msg.textContent = '';
      const pw = document.getElementById('loginPassword').value;
      global.Api.post('/api/v1/login', { password: pw }, { quiet: true })
        .then(() => { location.href = nextTarget(location.search); })
        .catch(err => {
          msg.textContent = err.status === 429 ? err.message : 'Passwort falsch.';
          const f = document.getElementById('loginPassword');
          if (f.select) f.select();
        });
    });
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Login = { nextTarget };
})(typeof window !== 'undefined' ? window : globalThis);
