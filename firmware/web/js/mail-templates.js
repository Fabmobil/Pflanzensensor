/**
 * @file mail-templates.js
 * @brief Editor der Mailvorlagen: Tabs, Byte-Zähler, Emojis, Platzhalter,
 *        Prüfhinweise, Live-Vorschau (sandboxed iframe), Speichern, Zurücksetzen
 */
(function (global) {
  'use strict';
  const { el, toast } = global.UI;
  const T = global.TemplateEngine;
  const KINDS = [['boot', 'Startmeldung'], ['warning', 'Warnung'], ['alive', 'Lebenszeichen'], ['style', 'Design']];
  /** 36 Emojis aus der Firmware 2.x */
  const EMOJIS = ['🌱', '🪴', '🌻', '🌼', '🌷', '🌵', '🍀', '🌿', '💧', '☀️', '🌤️', '🌧️', '🌡️', '💨', '🔥', '❄️',
                  '🚨', '⚠️', '✅', '❌', '🟢', '🟡', '🔴', '⚪', '😎', '😊', '🙂', '😟', '😱', '🥵', '🥶', '💚',
                  '🔁', '⏱️', '📶', '🗓️'];
  const PLACEHOLDERS = ['geraet', 'name', 'ip', 'ssid', 'neustarts', 'laufzeit', 'datum', 'uhrzeit', 'messwerte', 'auffaellige'];

  const state = { kind: 'boot', style: '', dirty: false };

  function counterClass(bytes, max) {
    if (bytes > max) return 'counter over';
    if (bytes > max * 0.9) return 'counter warn';
    return 'counter';
  }

  function insertAt(field, text) {
    const s = field.selectionStart || 0, e = field.selectionEnd || 0;
    field.value = field.value.slice(0, s) + text + field.value.slice(e);
    field.selectionStart = field.selectionEnd = s + text.length;
    field.focus();
    field.dispatchEvent(new Event('input'));
  }

  function update() {
    const subject = document.getElementById('tplSubject');
    const body = document.getElementById('tplBody');
    const isStyle = state.kind === 'style';
    const max = isStyle ? T.STIL_MAX : T.RUMPF_MAX;
    const b = T.byteLaenge(body.value);
    const c = document.getElementById('tplCounter');
    c.className = counterClass(b, max);
    c.textContent = b + ' / ' + max + ' Bytes';
    const sb = T.byteLaenge(subject.value);
    const sc = document.getElementById('tplSubjectCounter');
    sc.className = counterClass(sb, T.BETREFF_MAX);
    sc.textContent = sb + ' / ' + T.BETREFF_MAX + ' Bytes';
    const checks = document.getElementById('tplChecks');
    checks.textContent = '';
    const befunde = isStyle
      ? (/[<>]/.test(body.value) ? ['Spitze Klammern sind im Design nicht erlaubt.'] : [])
        .concat(b > max ? [`Zu lang: ${b} von ${max} Bytes.`] : [])
      : T.pruefe(body.value, max).befunde;
    befunde.forEach(t => checks.appendChild(el('li', { class: 'bad', text: t })));
    if (!befunde.length) checks.appendChild(el('li', { text: 'Keine Auffälligkeiten.' }));
    const frame = document.getElementById('tplPreview');
    const style = isStyle ? body.value : state.style;
    const previewBody = isStyle ? state.sampleBody || '' : body.value;
    const html = T.vorschau(previewBody, style);
    const subj = isStyle ? '' : T.expandiereBetreff(subject.value, T.BEISPIEL);
    document.getElementById('tplSubjectPreview').textContent = subj ? '[' + T.BEISPIEL.geraet + '] ' + subj : '';
    frame.srcdoc = html;
  }

  function load(kind) {
    state.kind = kind;
    document.querySelectorAll('.tabs .button').forEach(b => b.classList.toggle('active', b.dataset.kind === kind));
    document.getElementById('subjectGroup').style.display = kind === 'style' ? 'none' : '';
    return global.Api.get('/api/v1/mail/templates/' + kind).then(t => {
      document.getElementById('tplSubject').value = t.subject || '';
      document.getElementById('tplBody').value = t.body || '';
      document.getElementById('tplCustom').textContent = t.custom ? 'eigene Vorlage' : 'Standardvorlage';
      if (kind === 'style') state.style = t.body;
      update();
    });
  }

  function wire() {
    const tabs = document.getElementById('tplTabs');
    KINDS.forEach(([k, t]) => {
      const b = el('button', { class: 'button button-secondary', 'data-kind': k, text: t });
      b.addEventListener('click', () => load(k));
      tabs.appendChild(b);
    });
    const body = document.getElementById('tplBody');
    const pal = document.getElementById('tplEmojis');
    EMOJIS.forEach(e => {
      const b = el('button', { type: 'button', text: e });
      b.addEventListener('click', () => insertAt(body, e));
      pal.appendChild(b);
    });
    const ph = document.getElementById('tplPlaceholders');
    PLACEHOLDERS.forEach(p => {
      const b = el('button', { type: 'button', class: 'button button-secondary', text: '{' + p + '}' });
      b.addEventListener('click', () => insertAt(body, (p === 'messwerte' || p === 'auffaellige') ? '\n{' + p + '}\n' : '{' + p + '}'));
      ph.appendChild(b);
    });
    body.addEventListener('input', update);
    document.getElementById('tplSubject').addEventListener('input', update);
    document.getElementById('tplSave').addEventListener('click', () => {
      global.Api.put('/api/v1/mail/templates/' + state.kind, {
        subject: document.getElementById('tplSubject').value, body: body.value
      }).then(() => { toast('Vorlage gespeichert'); return load(state.kind); });
    });
    document.getElementById('tplReset').addEventListener('click', () => {
      global.UI.confirmDialog('Diese Vorlage auf den Standard zurücksetzen?').then(ok => {
        if (ok) global.Api.del('/api/v1/mail/templates/' + state.kind).then(() => { toast('Zurückgesetzt'); return load(state.kind); });
      });
    });
    // Stil und Beispieltext für die Vorschau vorab holen
    Promise.all([global.Api.get('/api/v1/mail/templates/style'), global.Api.get('/api/v1/mail/templates/alive')])
      .then(([s, a]) => {
        state.style = s.body;
        state.sampleBody = a.body;
        return load('boot');
      });
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
  global.MailTemplates = { counterClass, EMOJIS };
})(typeof window !== 'undefined' ? window : globalThis);
