/** Lädt web/js/devicewait.js kopflos (Loader aus den Tests der Firmware 2.x) */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { JS_DIR } from './load.mjs';

const SRC = path.join(JS_DIR, 'devicewait.js');

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

export function makeClock(start = 1_000_000) {
  return { now: start, advance(ms) { this.now += ms; } };
}

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

export function baselineOf(clock, uptime, extra = {}) {
  return { uptime, inUpdateMode: false, _at: clock.now, ...extra };
}
