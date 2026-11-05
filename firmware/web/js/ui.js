/**
 * @file ui.js
 * @brief Gemeinsame Bausteine der Oberfläche
 *
 * - toast(): kurze Rückmeldung
 * - bindAutosave(): speichert ein Feld nach 1 s Ruhe; der Debounce gilt JE FELD,
 *   zwei schnell geänderte Felder gehen beide durch (Befund B-50)
 * - Formatierer und die EINE Tabelle der Status-Texte (Befund B-52)
 */
(function (global) {
  'use strict';

  /** Status → Anzeigetext (Startseite, Admin, Mails nutzen dieselbe Tabelle) */
  const STATUS_TEXT = {
    green: 'OK',
    yellow: 'Warnung',
    red: 'Kritisch',
    error: 'Fehler',
    warmup: 'Aufwärmen',
    unknown: 'Unbekannt'
  };

  function statusText(s) {
    return STATUS_TEXT[s] || STATUS_TEXT.unknown;
  }

  function formatBytes(n) {
    if (n === null || n === undefined || isNaN(n)) return '–';
    if (n < 1024) return n + ' B';
    if (n < 1024 * 1024) return (n / 1024).toFixed(1) + ' KB';
    return (n / 1024 / 1024).toFixed(2) + ' MB';
  }

  /** "2d 3h 4m" wie im Altcode */
  function formatUptime(s) {
    s = Math.max(0, Math.floor(s || 0));
    const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
    return (d ? d + 'd ' : '') + (h ? h + 'h ' : '') + m + 'm';
  }

  function formatValue(v, unit) {
    if (v === null || v === undefined || isNaN(v)) return '--';
    return Number(v).toFixed(1) + (unit || '');
  }

  function el(tag, attrs, children) {
    const e = document.createElement(tag);
    Object.entries(attrs || {}).forEach(([k, v]) => {
      if (k === 'class') e.className = v;
      else if (k === 'text') e.textContent = v;
      else if (k.startsWith('on') && typeof v === 'function') e.addEventListener(k.slice(2), v);
      else if (v === true) e.setAttribute(k, '');
      else if (v !== false && v !== null && v !== undefined) e.setAttribute(k, v);
    });
    (children || []).forEach(c => e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c));
    return e;
  }

  let toastBox = null;
  function toast(message, kind) {
    if (typeof document === 'undefined') return;
    if (!toastBox) {
      toastBox = el('div', { class: 'toast-box', role: 'status', 'aria-live': 'polite' });
      document.body.appendChild(toastBox);
    }
    const t = el('div', { class: 'toast toast-' + (kind || 'success'), text: message });
    toastBox.appendChild(t);
    setTimeout(() => t.remove(), kind === 'error' ? 6000 : 3000);
  }

  /**
   * Feld automatisch speichern.
   * @param {HTMLElement} field
   * @param {function(value): Promise} save  bekommt den aktuellen Wert
   * @param {object} [o] {delay, read}
   */
  function bindAutosave(field, save, o) {
    o = o || {};
    const delay = o.delay === undefined ? 1000 : o.delay;
    const read = o.read || (f => f.type === 'checkbox' ? f.checked
      : (f.type === 'number' || f.type === 'range') ? (f.value === '' ? null : Number(f.value)) : f.value);
    let timer = null;
    const fire = () => {
      timer = null;
      field.classList.add('saving');
      Promise.resolve(save(read(field)))
        .then(() => {
          field.classList.remove('saving', 'invalid');
          if (!o.silent) toast(o.message || 'Gespeichert');
        })
        .catch(() => {
          field.classList.remove('saving');
          field.classList.add('invalid');
        });
    };
    const schedule = () => {
      if (timer) clearTimeout(timer);
      timer = setTimeout(fire, delay);
    };
    field.addEventListener(field.type === 'checkbox' || field.tagName === 'SELECT' ? 'change' : 'input', schedule);
    return { flush: () => { if (timer) { clearTimeout(timer); fire(); } } };
  }

  function confirmDialog(text) {
    return Promise.resolve(global.confirm ? global.confirm(text) : true);
  }

  /** Verschachtelten Patch aus Pfad "a.b" und Wert bauen */
  function patchFor(path, value) {
    const out = {};
    let cur = out;
    const parts = path.split('.');
    parts.forEach((p, i) => {
      if (i === parts.length - 1) cur[p] = value;
      else cur = cur[p] = {};
    });
    return out;
  }

  global.UI = { STATUS_TEXT, statusText, formatBytes, formatUptime, formatValue, el, toast,
                bindAutosave, confirmDialog, patchFor };
})(typeof window !== 'undefined' ? window : globalThis);
