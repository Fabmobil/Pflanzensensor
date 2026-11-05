/**
 * @file config-bind.js
 * @brief Felder mit data-path an /api/v1/config binden (laden + Autosave)
 */
(function (global) {
  'use strict';

  function get(obj, path) {
    return path.split('.').reduce((o, k) => (o ? o[k] : undefined), obj);
  }

  /** Felder füllen und Autosave anhängen. Gibt die geladene Antwort zurück. */
  function bindConfig(root, onSaved) {
    return global.Api.get('/api/v1/config').then(data => {
      (root || document).querySelectorAll('[data-path]').forEach(field => {
        const v = get(data.config, field.dataset.path);
        if (field.type === 'checkbox') field.checked = !!v;
        else if (v !== undefined) field.value = v;
        global.UI.bindAutosave(field, value => global.Api.patch('/api/v1/config',
          global.UI.patchFor(field.dataset.path, value)).then(r => {
            if (onSaved) onSaved(field.dataset.path, value, r);
            return r;
          }));
      });
      return data;
    });
  }

  global.ConfigBind = { bindConfig, get };
})(typeof window !== 'undefined' ? window : globalThis);
