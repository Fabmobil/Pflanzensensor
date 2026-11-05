/** Lädt web/js/chronik.js mit Fake-Canvas und Elementen (aus den 2.x-Tests übernommen) */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { makeElement } from './dom.mjs';
import { JS_DIR } from './load.mjs';

export function makeCanvas(width = 640, height = 320) {
  const calls = [];
  const record = (name) => (...args) => { calls.push([name, ...args]); };
  const ctx = {
    _calls: calls, font: '', lineWidth: 1, strokeStyle: '', fillStyle: '', textAlign: '',
    setTransform: record('setTransform'), clearRect: record('clearRect'), fillRect: record('fillRect'),
    strokeRect: record('strokeRect'), beginPath: record('beginPath'), closePath: record('closePath'),
    moveTo: record('moveTo'), lineTo: record('lineTo'), arc: record('arc'), stroke: record('stroke'),
    fill: record('fill'), fillText: record('fillText'), setLineDash: record('setLineDash'),
    save: record('save'), restore: record('restore')
  };
  const canvas = makeElement('canvas', []);
  canvas.clientWidth = width;
  canvas.clientHeight = height;
  canvas.width = width;
  canvas.height = height;
  canvas.getContext = () => ctx;
  canvas.getBoundingClientRect = () => ({ left: 0, top: 0, width, height });
  canvas._ctx = ctx;
  return canvas;
}

export function loadChronik({ respond = () => new Uint8Array(0), now = 1_000_000 } = {}) {
  const canvas = makeCanvas();
  const elemente = {
    'chronik-canvas': canvas,
    'chronik-legend': makeElement('div', ['chronik-legend']),
    'chronik-hint': makeElement('div', ['chronik-hint']),
    'chronik-ranges': makeElement('div', ['chronik-ranges']),
    'chronik-info': makeElement('table', [])
  };
  const fetches = [];
  const clock = { now };
  const doc = {
    visibilityState: 'visible',
    getElementById: (id) => elemente[id] || null,
    createElement: (tag) => makeElement(tag, []),
    querySelector: () => null,
    querySelectorAll: () => [],
    addEventListener: () => {}
  };
  const sandbox = {
    console: { log() {}, error() {}, warn() {} },
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)), clearTimeout,
    setInterval: () => 0, clearInterval: () => {},
    Date: new Proxy(Date, { get: (t, p) => (p === 'now' ? () => clock.now : Reflect.get(t, p)) }),
    document: doc, Map, Set, Uint8Array, DataView, ArrayBuffer, Number, Math, String, JSON,
    decodeURIComponent, escape, devicePixelRatio: 1,
    location: { pathname: '/chronik', host: 'testgerät' },
    fetch: async (url) => {
      fetches.push(url);
      const body = respond(url);
      if (body === null) throw new Error('nicht erreichbar');
      return { ok: true, status: 200, arrayBuffer: async () => body.buffer ?? body, json: async () => body };
    }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.window.addEventListener = () => {};
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, 'chronik.js'), 'utf8'), sandbox, { filename: 'chronik.js' });
  return Object.assign(sandbox.Chronik, { _document: doc, _elemente: elemente, _canvas: canvas, _fetches: fetches, _clock: clock });
}
