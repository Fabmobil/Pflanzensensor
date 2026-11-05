/** Echte HTML-Seiten + ihre Skripte: IDs vorhanden, Laden ohne Fehler */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { loadRealPage, idsUsedBy, settle, apiResponder } from './dom.mjs';
import { JS_DIR } from './load.mjs';

const PAGES = ['index.html', 'chronik.html', 'logs.html', 'admin.html', 'sensors.html', 'display.html',
               'mail.html', 'mail-templates.html', 'update.html', 'login.html'];
const DYNAMIC = new Set(['cloud-label', 'f-time', 'f-ssid', 'f-name', 'f-ip', 'f-rssi', 'f-uptime', 'f-reboots',
                         'f-version', 'f-build', 'nav-admin', 'footer-admin-menu', 'footer-stats-table',
                         'chronik-raw']);

for (const page of PAGES) {
  test(`${page}: jede gesuchte ID existiert`, () => {
    const p = loadRealPage(page);
    for (const s of p.scripts) {
      for (const id of idsUsedBy(s)) {
        if (DYNAMIC.has(id)) continue;
        assert.ok(p.document.getElementById(id), `${page}: #${id} (gesucht von ${s}) fehlt`);
      }
    }
  });

  test(`${page}: lädt mit den echten Backend-Antworten ohne Fehler`, async () => {
    const errors = [];
    const p = loadRealPage(page, { respond: apiResponder(),
                                   location: { pathname: '/' + page.replace('.html', ''), host: 't', reload() {} },
                                   extra: { console: { log() {}, warn() {}, error: (...a) => errors.push(a.join(' ')) } } });
    p.start();
    await settle(40);
    assert.deepEqual(errors, []);
    // Keine Fehlermeldung als Toast
    const toasts = p.document.querySelectorAll('.toast-error');
    assert.equal(toasts.length, 0, toasts.map((t) => t.textContent).join(' | '));
    assert.equal(p.document.getElementById('f-version').textContent, 'V 3.0.0');
  });
}

test('jede Seite bindet ui.js, api.js und footer.js zuerst ein', () => {
  for (const page of PAGES) {
    const p = loadRealPage(page);
    assert.deepEqual(p.scripts.slice(0, 3), ['ui.js', 'api.js', 'footer.js'], page);
    for (const s of p.scripts) assert.ok(fs.existsSync(path.join(JS_DIR, s)), `${page}: ${s} fehlt`);
  }
});

test('jede verlinkte CSS- und Bilddatei existiert', () => {
  const web = path.resolve(JS_DIR, '..');
  for (const page of PAGES) {
    const html = fs.readFileSync(path.join(web, page), 'utf8');
    for (const m of html.matchAll(/(?:href|src)="(\/(?:css|img)\/[^"]+)"/g))
      assert.ok(fs.existsSync(path.join(web, m[1])), `${page}: ${m[1]}`);
  }
  for (const f of fs.readdirSync(JS_DIR)) {
    const src = fs.readFileSync(path.join(JS_DIR, f), 'utf8');
    for (const m of src.matchAll(/'\/img\/([^']+)'/g)) assert.ok(fs.existsSync(path.join(web, 'img', m[1])), `${f}: ${m[1]}`);
  }
});

test('Seitenpfade der Links stimmen mit der Routentabelle überein', () => {
  const router = fs.readFileSync(path.resolve(JS_DIR, '../../src/web/router.cpp'), 'utf8');
  const pages = [...router.matchAll(/\{"(\/[^"]*)", "(\/[a-z-]+\.html)"/g)].map((m) => m[1]);
  const routes = [...router.matchAll(/\{"(\/[^"]*)", M/g)].map((m) => m[1]);
  const known = (u) => pages.includes(u) || routes.some((r) => u === r || (r.endsWith('/') && u.startsWith(r)));
  for (const f of fs.readdirSync(JS_DIR).concat(PAGES.map((p) => '../' + p))) {
    const src = fs.readFileSync(path.join(JS_DIR, f), 'utf8');
    for (const m of src.matchAll(/(?:href=|['"`])(\/(?:admin|api|logs|chronik|getLatestValues|status)[^'"`?\s$]*)/g)) {
      const u = m[1].replace(/\/+$/, '') || '/';
      assert.ok(known(u) || known(u + '/'), `${f}: ${u} ist keine Route`);
    }
  }
});
