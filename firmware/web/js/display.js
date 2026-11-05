/**
 * @file display.js
 * @brief Seite Display: Dauer, Uhrformat, Bildschirme, Kanäle auf dem Display
 *        (aus der Konfiguration, nicht aus Laufzeitdaten - B-36)
 */
(function (global) {
  'use strict';
  const { el } = global.UI;

  function wire() {
    global.ConfigBind.bindConfig(document);
    global.Api.get('/api/v1/sensors').then(data => {
      const box = document.getElementById('displayChannels');
      data.sensors.forEach(s => s.channels.forEach(ch => {
        const cb = el('input', { type: 'checkbox', checked: ch.display });
        global.UI.bindAutosave(cb, v => global.Api.patch('/api/v1/sensors',
          { [s.id]: { channels: { [String(ch.index)]: { display: v } } } }), { delay: 300 });
        const label = el('label', {}, [cb, ' ' + ch.name + ' (' + ch.key + ')' + (ch.enabled && s.enabled ? '' : ' – deaktiviert')]);
        box.appendChild(el('div', {}, [label]));
      }));
    });
  }

  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', wire);
})(typeof window !== 'undefined' ? window : globalThis);
