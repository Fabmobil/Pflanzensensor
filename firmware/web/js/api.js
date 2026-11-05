/**
 * @file api.js
 * @brief Zugriff auf die JSON-API mit einheitlicher Fehlerbehandlung
 *
 * Fehlerformat des Geräts: {"ok":false,"error":"<code>","message":"…","fields":{…}}
 * Bei 401 geht es zur Anmeldeseite (nur Passwort, Sitzungs-Cookie) und
 * danach zurück.
 */
(function (global) {
  'use strict';

  class ApiError extends Error {
    constructor(status, body) {
      super((body && body.message) || ('HTTP ' + status));
      this.status = status;
      this.body = body || {};
      this.fields = (body && body.fields) || {};
    }
  }

  function request(method, path, body, opts) {
    const init = { method, headers: {}, credentials: 'same-origin' };
    if (body !== undefined && body !== null) {
      init.headers['Content-Type'] = 'application/json';
      init.body = typeof body === 'string' ? body : JSON.stringify(body);
    }
    return fetch(path, init).then(resp => {
      const type = resp.headers.get('Content-Type') || '';
      const parse = type.indexOf('json') >= 0 ? resp.json().catch(() => ({})) : resp.text();
      return parse.then(data => {
        if (!resp.ok) {
          const err = new ApiError(resp.status, typeof data === 'object' ? data : { message: data });
          if (!(opts && opts.quiet)) report(err);
          throw err;
        }
        return data;
      });
    });
  }

  function report(err) {
    if (!global.UI) return;
    if (err.status === 401) {
      global.UI.toast('Bitte anmelden.', 'error');
      if (global.location && global.location.pathname !== '/login')
        global.location.href = '/login?next=' + encodeURIComponent(global.location.pathname);
    } else if (err.status === 429) {
      global.UI.toast(err.message || 'Bitte kurz warten.', 'warning');
    } else {
      global.UI.toast(err.message, 'error');
    }
  }

  global.Api = {
    ApiError,
    get: (p, o) => request('GET', p, null, o),
    post: (p, b, o) => request('POST', p, b === undefined ? null : b, o),
    put: (p, b, o) => request('PUT', p, b, o),
    patch: (p, b, o) => request('PATCH', p, b, o),
    del: (p, o) => request('DELETE', p, null, o)
  };
})(typeof window !== 'undefined' ? window : globalThis);
