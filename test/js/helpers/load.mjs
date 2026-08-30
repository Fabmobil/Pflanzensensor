/**
 * Lädt data/js/devicewait.js in eine frische Sandbox.
 *
 * Die Datei ist ein klassisches Skript (kein Modul) und hängt sich an window.
 * Statt sie umzubauen, wird sie hier in einem vorbereiteten globalen Kontext
 * ausgeführt - so bleibt die getestete Datei exakt die, die auch ausgeliefert
 * wird.
 *
 * Die Uhr ist steuerbar: Date.now() kommt aus einem Zähler, den der Test
 * weiterdreht. Dadurch lassen sich Bootzeiten von einer Minute in
 * Millisekunden durchspielen, und die Prüfung der Uptime-Drift wird
 * deterministisch statt von echter Zeit abhängig.
 */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const JS_DIR = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '../../../Pflanzensensor/data/js'
);
const SRC = path.join(JS_DIR, 'devicewait.js');

/** Minimales DOM, das createOverlay bedienen kann. */
function makeDocument() {
  const mkEl = () => {
    const el = {
      style: {}, children: [], className: '', type: '', textContent: '', id: '',
      parentNode: null, _listeners: {},
      appendChild(c) { c.parentNode = el; el.children.push(c); return c; },
      removeChild(c) { el.children = el.children.filter((x) => x !== c); c.parentNode = null; },
      addEventListener(ev, fn) { (el._listeners[ev] ||= []).push(fn); },
      click() { (el._listeners.click || []).forEach((f) => f()); }
    };
    return el;
  };
  const body = mkEl();
  return {
    createElement: mkEl,
    body,
    // Kopflos gibt es keine Seite - die Init-IIFEs in admin.js suchen ihre
    // Formulare, finden nichts und steigen aus; genau das wollen wir.
    // Gesucht wird trotzdem im aufgebauten Baum, damit Code, der ein Element
    // anlegt und beim nächsten Aufruf wiederverwendet (showSuccessMessage /
    // showErrorMessage), auch wirklich wiederverwendet statt zu verdoppeln.
    getElementById(id) {
      let hit = null;
      (function walk(n) {
        if (hit) return;
        if (n.id === id) { hit = n; return; }
        n.children.forEach(walk);
      })(body);
      return hit;
    },
    querySelector: () => null,
    querySelectorAll: () => [],
    addEventListener: () => {},
    _findByClass(cls) {
      const out = [];
      (function walk(n) { if (n.className === cls) out.push(n); n.children.forEach(walk); })(body);
      return out;
    }
  };
}

/** Steuerbare Uhr. Bewusst ausserhalb von load(), damit Test und Sandbox
 *  dieselbe benutzen - sonst dreht der Test eine Uhr weiter, die die getestete
 *  Datei gar nicht liest, und waitForDevice erreicht seine Frist nie. */
export function makeClock(start = 1_000_000) {
  return { now: start, advance(ms) { this.now += ms; } };
}

/**
 * @param {object} o
 * @param {Function} o.respond  () => statusObjekt | null (null = nicht erreichbar)
 * @param {object}   o.clock    Uhr aus makeClock(); sonst wird eine angelegt
 * @returns {{DeviceWait, clock, document, reloads, statusCalls}}
 */
export function load({ respond = () => null, clock = makeClock() } = {}) {
  const reloads = [];
  const state = { statusCalls: 0 };

  const sandbox = {
    console,
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)), // Wartezeiten raffen
    clearTimeout,
    AbortController,
    Date: new Proxy(Date, { get: (t, p) => (p === 'now' ? () => clock.now : Reflect.get(t, p)) }),
    document: makeDocument(),
    location: { host: 'testgerät', reload: () => reloads.push(clock.now) },
    fetch: async () => {
      state.statusCalls++;
      const body = respond();
      if (body === null) throw new Error('nicht erreichbar');
      return { ok: true, json: async () => structuredClone(body) };
    }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;

  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(SRC, 'utf8'), sandbox, { filename: SRC });

  return {
    DeviceWait: sandbox.DeviceWait,
    clock,
    document: sandbox.document,
    reloads,
    get statusCalls() { return state.statusCalls; }
  };
}

/** Basiswert bauen, wie getStatus() ihn liefern würde (inkl. Zeitstempel). */
export function baselineOf(clock, uptime, extra = {}) {
  return { uptime, inUpdateMode: false, _at: clock.now, ...extra };
}

/**
 * Lädt data/js/admin.js kopflos.
 *
 * Die Datei registriert beim Laden nur window-load-Handler und zwei IIFEs, die
 * über document.getElementById() nach Formularen suchen und ohne Treffer
 * sofort aussteigen - kopflos also unkritisch. Damit lassen sich die
 * seitenübergreifenden Helfer direkt prüfen, allen voran parseJsonResponse():
 * die lag früher doppelt vor (auch in admin_display.js) und wurde dort von der
 * später geladenen, schwächeren Fassung verdrängt.
 */
export function loadAdmin() {
  const messages = [];
  const sandbox = {
    console,
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)),
    clearTimeout,
    document: makeDocument(),
    fetch: async () => { throw new Error('im Test nicht benutzt'); },
    location: { host: 'testgerät', reload() {} }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.window.addEventListener = () => {};
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, 'admin.js'), 'utf8'), sandbox,
                  { filename: 'admin.js' });

  // Meldungen mitschneiden statt ins DOM zu schreiben
  const realError = sandbox.showErrorMessage;
  sandbox.showErrorMessage = (m) => { messages.push(['error', m]); realError(m); };
  return { sandbox, messages };
}

/** Antwort im Stil von fetch() bauen. */
export function fakeResponse({ status = 200, contentType = 'application/json', body = '{}' } = {}) {
  return {
    status,
    headers: { get: (h) => (h.toLowerCase() === 'content-type' ? contentType : null) },
    text: async () => body,
    json: async () => JSON.parse(body)
  };
}

/**
 * Lädt data/js/admin_sensors.js kopflos und gibt window.AdminSensors zurück.
 *
 * Die Datei exportiert ihre Module ausdrücklich "for testing" - hier wird
 * genau das benutzt. Die Initialisierung am Dateiende hängt sich über
 * onDomReady() an DOMContentLoaded; mit readyState "loading" und einem
 * addEventListener, das nichts tut, läuft sie nicht an. Getestet werden
 * dadurch die reinen Rechenteile, ohne dass Poller oder Ereignisbindungen
 * starten.
 */
export function loadAdminSensors() {
  const doc = makeDocument();
  doc.readyState = 'loading';
  doc.visibilityState = 'visible';
  const fetches = [];
  const sandbox = {
    console,
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)),
    clearTimeout, setInterval: () => 0, clearInterval: () => {},
    document: doc,
    URLSearchParams,
    FormData: globalThis.FormData,
    fetch: async (url) => { fetches.push(url); return { ok: true, status: 200,
      headers: { get: () => 'application/json' }, json: async () => ({ sensors: {} }),
      text: async () => '{}' }; },
    location: { host: 'testgerät', reload() {} }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.window.addEventListener = () => {};
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, 'admin_sensors.js'), 'utf8'), sandbox,
                  { filename: 'admin_sensors.js' });
  return Object.assign(sandbox.AdminSensors, { _document: doc, _fetches: fetches });
}
