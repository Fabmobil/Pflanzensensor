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
    // admin_sensors.js setzt admin.js voraus und benutzt dessen Helfer. Ohne
    // sie laeuft der Poller zwar, verschluckt sich aber an einem
    // ReferenceError - die Zusicherung auf die Zahl der Abrufe hielte zwar,
    // die Testausgabe waere aber voller irrefuehrender Fehler.
    parseJsonResponse: (r) => r.json(),
    showErrorMessage: () => {},
    showSuccessMessage: () => {},
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

// === Reicheres Fake-DOM für sensors.js ===
//
// makeDocument() oben reicht für devicewait.js und admin.js, kennt aber weder
// classList noch dataset noch querySelector. sensors.js lebt genau davon: es
// sucht Zeilen über Attributselektoren, schaltet Statusklassen um und merkt
// sich Zeitstempel im Datensatz. Deshalb hier ein zweiter, etwas
// ausführlicherer Nachbau - bewusst nur so viel, wie die Datei benutzt.

/** "data-last-measurement" -> "lastMeasurement" */
function datasetKey(attr) {
  return attr.slice(5).replace(/-([a-z])/g, (_, c) => c.toUpperCase());
}

function getAttr(el, name) {
  if (name.startsWith('data-')) return el.dataset[datasetKey(name)];
  if (name === 'class') return [...el.classList._set].join(' ');
  return el._attrs[name];
}

/** Einfacher Teilselektor: tag, .klasse(n), [attr], [attr="wert"] - kombinierbar. */
function matchesSimple(el, part) {
  const attrRe = /\[([a-zA-Z0-9_-]+)(?:=["']?([^"'\]]*)["']?)?\]/g;
  let rest = part;
  let m;
  while ((m = attrRe.exec(part)) !== null) {
    const value = getAttr(el, m[1]);
    if (value === undefined || value === null || value === '') return false;
    if (m[2] !== undefined && String(value) !== m[2]) return false;
  }
  rest = rest.replace(attrRe, '');

  const classes = [...rest.matchAll(/\.([a-zA-Z0-9_-]+)/g)].map((c) => c[1]);
  if (classes.some((c) => !el.classList.contains(c))) return false;

  const tag = rest.replace(/\.[a-zA-Z0-9_-]+/g, '');
  if (tag && el.tagName !== tag.toUpperCase()) return false;
  return true;
}

/** Nachfahrenkette wie ".sensor .interval span" - von rechts nach links geprüft. */
function matchesSelector(el, selector) {
  const parts = selector.trim().split(/\s+/);
  if (!matchesSimple(el, parts[parts.length - 1])) return false;
  let node = el.parentNode;
  for (let i = parts.length - 2; i >= 0; i--) {
    while (node && !matchesSimple(node, parts[i])) node = node.parentNode;
    if (!node) return false;
    node = node.parentNode;
  }
  return true;
}

function descendants(root) {
  const out = [];
  (function walk(n) { n.children.forEach((c) => { out.push(c); walk(c); }); })(root);
  return out;
}

export function makeElement(tag = 'div', classNames = []) {
  const set = new Set(classNames);
  const el = {
    tagName: tag.toUpperCase(),
    children: [],
    parentNode: null,
    dataset: {},
    _attrs: {},
    _listeners: {},
    textContent: '',
    classList: {
      _set: set,
      add: (...c) => c.forEach((x) => set.add(x)),
      remove: (...c) => c.forEach((x) => set.delete(x)),
      contains: (c) => set.has(c)
    },
    appendChild(child) { child.parentNode = el; el.children.push(child); return child; },
    // Nur der Setter wird gebraucht: chronik.js leert damit die Legende, bevor
    // es sie neu aufbaut. Der Getter liefert bewusst nichts Erfundenes.
    set innerHTML(html) { el.children = []; el._html = html; },
    get innerHTML() { return el._html || ''; },
    setAttribute(name, value) { el._attrs[name] = String(value); },
    removeAttribute(name) { delete el._attrs[name]; },
    getAttribute(name) { return getAttr(el, name); },
    addEventListener(type, fn) { (el._listeners[type] ||= []).push(fn); },
    querySelector(sel) { return descendants(el).find((n) => matchesSelector(n, sel)) || null; },
    querySelectorAll(sel) { return descendants(el).filter((n) => matchesSelector(n, sel)); },
    closest(sel) {
      let node = el;
      while (node) { if (matchesSelector(node, sel)) return node; node = node.parentNode; }
      return null;
    },
    /** Ereignis auslösen und wie im Browser nach oben durchreichen - nur so
     *  erreicht ein Klick auf das Blatt den Zuhörer am Container. */
    dispatch(type, init = {}) {
      const event = { type, target: el, defaultPrevented: false, ...init,
                      preventDefault() { event.defaultPrevented = true; } };
      let node = el;
      while (node) {
        (node._listeners[type] || []).slice().forEach((fn) => fn(event));
        node = node.parentNode;
      }
      return event;
    }
  };
  return el;
}

/**
 * Baut den Sensorbereich der Startseite so nach, wie generateSensorBox() ihn
 * liefert: .sensors-container > .sensor[data-sensor][data-sensor-id] >
 * (.leaf-wrap > .leaf, .card > (.value>span, .status>span, .interval>span)).
 */
export function makeSensorDocument(rows) {
  const root = makeElement('div', ['page']);
  const container = makeElement('div', ['sensors-container']);
  root.appendChild(container);

  const box = makeElement('div', ['box', 'status-unknown']);
  root.appendChild(box);
  box.appendChild(makeElement('img', ['face']));

  rows.forEach((row) => {
    const sensor = makeElement('div', ['sensor', row.side || 'left',
                                       `sensor-status-${row.status || 'green'}`]);
    sensor.dataset.sensor = row.key;
    if (row.sensorId !== null) sensor.dataset.sensorId = row.sensorId ?? row.key.replace(/_\d+$/, '');
    const wrap = makeElement('span', ['leaf-wrap']);
    wrap.appendChild(makeElement('img', ['leaf']));
    sensor.appendChild(makeElement('img', ['stem']));
    sensor.appendChild(wrap);
    const card = makeElement('div', ['card']);
    ['value', 'status', 'interval'].forEach((name) => {
      const field = makeElement('div', [name]);
      field.appendChild(makeElement('span'));
      card.appendChild(field);
    });
    sensor.appendChild(card);
    container.appendChild(sensor);
  });

  return {
    root,
    readyState: 'loading',
    visibilityState: 'visible',
    body: root,
    createElement: (tag) => makeElement(tag),
    getElementById: (id) => descendants(root).find((n) => n._attrs.id === id) || null,
    querySelector: (sel) => root.querySelector(sel),
    querySelectorAll: (sel) => root.querySelectorAll(sel),
    addEventListener: () => {}
  };
}

/**
 * Lädt data/js/sensors.js kopflos und gibt window.Sensors zurück.
 *
 * Wie bei loadAdminSensors() verhindert ein window.addEventListener, das nichts
 * tut, dass der DOMContentLoaded-Block mit seinen Zeitgebern anläuft - geprüft
 * werden die einzelnen Funktionen, nicht der Aufbau der Seite.
 *
 * @param {object} o
 * @param {Array}  o.rows     Zeilen für makeSensorDocument()
 * @param {Function} o.respond (url, options) => {ok, status, body} | wirft
 * @param {object} o.clock    Uhr aus makeClock()
 */
export function loadSensors({ rows = [], respond = () => ({ ok: true, status: 200, body: {} }),
                              clock = makeClock() } = {}) {
  const doc = makeSensorDocument(rows);
  const fetches = [];
  const sandbox = {
    console: { log() {}, error() {}, warn() {} }, // Testausgabe nicht zumüllen
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)),
    clearTimeout,
    setInterval: () => 0,
    clearInterval: () => {},
    Date: new Proxy(Date, { get: (t, p) => (p === 'now' ? () => clock.now : Reflect.get(t, p)) }),
    document: doc,
    location: { pathname: '/', host: 'testgerät', reload() {} },
    performance: { now: () => clock.now },
    requestAnimationFrame: () => 0,
    fetch: async (url, options = {}) => {
      fetches.push({ url, options });
      const answer = respond(url, options); // darf werfen: Netzwerkfehler
      return {
        ok: answer.ok !== false,
        status: answer.status ?? 200,
        json: async () => structuredClone(answer.body ?? {})
      };
    }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.window.addEventListener = () => {};
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, 'sensors.js'), 'utf8'), sandbox,
                  { filename: 'sensors.js' });

  return Object.assign(sandbox.Sensors, { _document: doc, _fetches: fetches, _clock: clock,
                                          _window: sandbox });
}

/** Fake-Canvas: protokolliert alle Zeichenaufrufe, statt zu malen. */
export function makeCanvas(width = 640, height = 320) {
  const calls = [];
  const record = (name) => (...args) => { calls.push([name, ...args]); };
  const ctx = {
    _calls: calls,
    font: '', lineWidth: 1, strokeStyle: '', fillStyle: '', textAlign: '',
    setTransform: record('setTransform'), clearRect: record('clearRect'),
    fillRect: record('fillRect'), strokeRect: record('strokeRect'),
    beginPath: record('beginPath'), closePath: record('closePath'),
    moveTo: record('moveTo'), lineTo: record('lineTo'), arc: record('arc'),
    stroke: record('stroke'), fill: record('fill'), fillText: record('fillText'),
    setLineDash: record('setLineDash'), save: record('save'), restore: record('restore')
  };
  const canvas = makeElement('canvas', []);
  canvas.clientWidth = width;
  canvas.clientHeight = height;
  canvas.width = width;
  canvas.height = height;
  canvas.getContext = () => ctx;
  canvas.getBoundingClientRect = () => ({ left: 0, top: 0, width, height });
  canvas.setPointerCapture = () => {};
  canvas._ctx = ctx;
  return canvas;
}

/**
 * Lädt data/js/chronik.js kopflos und gibt window.Chronik zurück.
 *
 * Wie bei loadSensors() verhindert ein window.addEventListener, das nichts
 * tut, dass der DOMContentLoaded-Block anläuft und Daten holen will. Geprüft
 * werden die reinen Funktionen sowie ein Zeichendurchlauf gegen den
 * Fake-Canvas.
 */
export function loadChronik({ respond = () => new Uint8Array(0), clock = makeClock() } = {}) {
  const canvas = makeCanvas();
  const elemente = {
    'chronik-canvas': canvas,
    'chronik-legend': makeElement('div', ['chronik-legend']),
    'chronik-hint': makeElement('div', ['chronik-hint']),
    'chronik-ranges': makeElement('div', ['chronik-ranges']),
    'chronik-raw': makeElement('input', [])
  };
  elemente['chronik-hint'].style = {};
  elemente['chronik-raw'].checked = false;

  const fetches = [];
  const doc = {
    readyState: 'loading',
    visibilityState: 'visible',
    getElementById: (id) => elemente[id] || null,
    createElement: (tag) => {
      const el = makeElement(tag, []);
      el.style = {};
      return el;
    },
    querySelector: () => null,
    querySelectorAll: () => [],
    addEventListener: () => {}
  };

  const sandbox = {
    console: { log() {}, error() {}, warn() {} },
    setTimeout: (fn, ms) => setTimeout(fn, Math.min(ms ?? 0, 5)),
    clearTimeout,
    setInterval: () => 0,
    clearInterval: () => {},
    Date: new Proxy(Date, { get: (t, p) => (p === 'now' ? () => clock.now : Reflect.get(t, p)) }),
    document: doc,
    Map, Set, Uint8Array, DataView, ArrayBuffer, Number, Math, String, JSON,
    decodeURIComponent, escape,
    devicePixelRatio: 1,
    location: { pathname: '/chronik', host: 'testgerät' },
    fetch: async (url) => {
      fetches.push(url);
      const body = respond(url);
      if (body === null) throw new Error('nicht erreichbar');
      return { ok: true, status: 200, arrayBuffer: async () => body.buffer ?? body };
    }
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.window.addEventListener = () => {};

  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, 'chronik.js'), 'utf8'), sandbox,
                  { filename: 'chronik.js' });

  return Object.assign(sandbox.Chronik, {
    _document: doc, _elemente: elemente, _canvas: canvas, _fetches: fetches, _clock: clock
  });
}
