/**
 * Lädt Skripte aus firmware/web/js in eine frische Sandbox (ohne DOM).
 * Die Skripte hängen sich an window bzw. globalThis; ausgeliefert wird exakt
 * dieselbe Datei.
 */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const JS_DIR = path.resolve(here, '../../web/js');
export const VECTORS = path.resolve(here, '../vectors');

export function vector(name) {
  return JSON.parse(fs.readFileSync(path.join(VECTORS, name), 'utf8'));
}

/** Uhr und Timer, die der Test weiterdreht */
export function makeClock(start = 1000) {
  let now = start;
  let timers = [];
  let id = 1;
  return {
    now: () => now,
    setTimeout(fn, ms) { const t = { id: id++, at: now + ms, fn }; timers.push(t); return t.id; },
    clearTimeout(i) { timers = timers.filter(t => t.id !== i); },
    advance(ms) {
      const until = now + ms;
      for (;;) {
        timers.sort((a, b) => a.at - b.at);
        const t = timers[0];
        if (!t || t.at > until) break;
        timers.shift();
        now = t.at;
        t.fn();
      }
      now = until;
    }
  };
}

export function load(files, extra = {}) {
  const clock = extra.clock || makeClock();
  const ctx = {
    console, Promise, JSON, Math, Number, String, Array, Object, Map, Set, RegExp, Error, DataView,
    Uint8Array, ArrayBuffer, isNaN, isFinite, parseInt, parseFloat, encodeURIComponent, decodeURIComponent, escape,
    Date: { now: clock.now },
    setTimeout: clock.setTimeout, clearTimeout: clock.clearTimeout, setInterval: () => 0, clearInterval: () => {},
    performance: { now: clock.now },
    ...extra.globals
  };
  ctx.window = ctx;
  ctx.globalThis = ctx;
  vm.createContext(ctx);
  for (const f of files) {
    vm.runInContext(fs.readFileSync(path.join(JS_DIR, f), 'utf8'), ctx, { filename: f });
  }
  return { ctx, clock };
}
