/**
 * @file logs.js
 * @brief Live-Log: pollt GET /api/v1/logs?since=<seq> jede Sekunde
 *
 * Ersetzt den WebSocket der Firmware 2.x. Auto-Scroll schaltet sich ab, wenn
 * man hochscrollt, und wieder an, wenn man ans Ende scrollt. Höchstens 100
 * Einträge. Nach Fehlern bis zu 5 Wiederholungen mit 3 s × Versuch Abstand.
 * Die Ansicht wird nicht mehr periodisch geleert (B-55). Log-Level und
 * Debug-Schalter gehen ohne Anmeldung an /api/v1/logs/settings.
 */
(function (global) {
  'use strict';
  const { el } = global.UI;
  const MAX_ENTRIES = 100;
  const POLL_MS = 1000;
  const MAX_RETRIES = 5;
  const SETTINGS = '/api/v1/logs/settings';

  const state = { since: 0, autoScroll: true, failures: 0, timer: null, stopped: false };

  /** Neue Zeilen anhängen und kürzen (rein, getestet) */
  function merge(list, lines, max) {
    const out = list.concat(lines);
    return out.length > max ? out.slice(out.length - max) : out;
  }

  function setStatus(text, ok) {
    const s = document.getElementById('logStatus');
    s.textContent = text;
    s.className = 'ws-status ' + (ok ? 'connected' : 'disconnected');
  }

  function append(data) {
    const box = document.getElementById('logBox');
    if (data.dropped > 0 && state.since > 0) {
      box.appendChild(el('div', { class: 'log-entry system', text: `--- ${data.dropped} Zeilen verpasst ---` }));
    }
    data.lines.forEach(l => {
      box.appendChild(el('div', { class: 'log-entry ' + l.level.toLowerCase() }, [
        el('span', { class: 'ts', text: l.ts }), el('span', { text: `[${l.module}] ${l.text}` })]));
    });
    while (box.children.length > MAX_ENTRIES) box.removeChild(box.firstChild);
    if (state.autoScroll) box.scrollTop = box.scrollHeight;
  }

  function schedule(ms) {
    clearTimeout(state.timer);
    state.timer = setTimeout(poll, ms);
  }

  function poll() {
    if (document.visibilityState === 'hidden') return schedule(POLL_MS);
    global.Api.get('/api/v1/logs?since=' + state.since, { quiet: true }).then(d => {
      state.failures = 0;
      setStatus('verbunden', true);
      append(d);
      state.since = d.next;
      schedule(POLL_MS);
    }).catch(() => {
      state.failures++;
      if (state.failures > MAX_RETRIES) {
        setStatus('getrennt - Seite neu laden', false);
        return;
      }
      setStatus(`getrennt - neuer Versuch ${state.failures}/${MAX_RETRIES}`, false);
      schedule(3000 * state.failures);
    });
  }

  function wire() {
    const box = document.getElementById('logBox');
    box.addEventListener('scroll', () => {
      state.autoScroll = box.scrollTop + box.clientHeight >= box.scrollHeight - 10;
      document.getElementById('autoScroll').checked = state.autoScroll;
    });
    document.getElementById('autoScroll').addEventListener('change', e => { state.autoScroll = e.target.checked; });
    document.getElementById('logClear').addEventListener('click', () => { box.textContent = ''; });
    // Debug-Schalter wirken nur bei Level DEBUG (sonst schneidet das Level ab)
    const markLevel = level => {
      document.querySelectorAll('.log-level-btn').forEach(x => x.classList.toggle('active', x.dataset.level === level));
      document.querySelectorAll('[data-log]').forEach(f => { f.disabled = level !== 'DEBUG'; });
    };
    const saveSettings = log => global.Api.patch(SETTINGS, { log });
    document.querySelectorAll('.log-level-btn').forEach(b => b.addEventListener('click', () => {
      saveSettings({ level: b.dataset.level }).then(() => {
        markLevel(b.dataset.level);
        global.UI.toast('Log-Level ' + b.textContent);
      });
    }));
    // Level und Debug-Schalter sind wie die Logs selbst ohne Anmeldung
    // änderbar (eigener Endpunkt, der nur diese Felder annimmt).
    global.Api.get(SETTINGS, { quiet: true }).then(d => {
      document.querySelectorAll('[data-log]').forEach(f => {
        f.checked = !!d.log[f.dataset.log];
        global.UI.bindAutosave(f, v => saveSettings({ [f.dataset.log]: v }), { delay: 0 });
      });
      markLevel(d.log.level);
    }).catch(() => {});
    poll();
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.Logs = { merge, state };
})(typeof window !== 'undefined' ? window : globalThis);
