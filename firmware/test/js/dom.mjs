/**
 * Mini-DOM für die Tests der Weboberfläche (ohne Abhängigkeiten).
 * Unterstützt: Elementbaum, Klassen, data-Attribute und dataset, einfache Selektoren
 * (tag, .klasse, [attr], [attr="wert"], #id, Nachfahren), Ereignisse mit
 * Bubbling, getElementById, value/checked/disabled, style.
 * Die Selektorlogik stammt aus den Tests der Firmware 2.x.
 */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { JS_DIR, makeClock } from './load.mjs';

function datasetKey(attr) {
  return attr.slice(5).replace(/-([a-z])/g, (_, c) => c.toUpperCase());
}

function getAttr(el, name) {
  if (name.startsWith('data-')) return el.dataset[datasetKey(name)];
  if (name === 'class') return [...el.classList._set].join(' ');
  if (name === 'id') return el.id;
  return el._attrs[name];
}

function matchesSimple(el, part) {
  const attrRe = /\[([a-zA-Z0-9_-]+)(?:=["']?([^"'\]]*)["']?)?\]/g;
  let m;
  while ((m = attrRe.exec(part)) !== null) {
    const value = getAttr(el, m[1]);
    if (value === undefined || value === null) return false;
    if (m[2] !== undefined && String(value) !== m[2]) return false;
  }
  let rest = part.replace(attrRe, '');
  const id = /#([a-zA-Z0-9_-]+)/.exec(rest);
  if (id && el.id !== id[1]) return false;
  rest = rest.replace(/#[a-zA-Z0-9_-]+/, '');
  const classes = [...rest.matchAll(/\.([a-zA-Z0-9_-]+)/g)].map((c) => c[1]);
  if (classes.some((c) => !el.classList.contains(c))) return false;
  const tag = rest.replace(/\.[a-zA-Z0-9_-]+/g, '').replace(/:checked$/, '');
  if (part.endsWith(':checked') && !el.checked) return false;
  if (tag && el.tagName !== tag.toUpperCase()) return false;
  return true;
}

function matchesSelector(el, selector) {
  return selector.split(',').some((sel) => {
    const parts = sel.trim().split(/\s+/);
    if (!matchesSimple(el, parts[parts.length - 1])) return false;
    let node = el.parentNode;
    for (let i = parts.length - 2; i >= 0; i--) {
      while (node && !matchesSimple(node, parts[i])) node = node.parentNode;
      if (!node) return false;
      node = node.parentNode;
    }
    return true;
  });
}

function descendants(root) {
  const out = [];
  (function walk(n) { n.children.forEach((c) => { out.push(c); walk(c); }); })(root);
  return out;
}

export function makeElement(tag = 'div', classNames = []) {
  const set = new Set(classNames);
  let text = '';
  const el = {
    tagName: tag.toUpperCase(),
    children: [],
    parentNode: null,
    dataset: {},
    style: {},
    _attrs: {},
    _listeners: {},
    id: '',
    value: '',
    checked: false,
    disabled: false,
    type: '',
    scrollTop: 0,
    scrollHeight: 0,
    clientHeight: 0,
    files: [],
    get className() { return [...set].join(' '); },
    set className(v) { set.clear(); String(v).split(/\s+/).filter(Boolean).forEach((c) => set.add(c)); },
    classList: {
      _set: set,
      add: (...c) => c.forEach((x) => set.add(x)),
      remove: (...c) => c.forEach((x) => set.delete(x)),
      contains: (c) => set.has(c),
      toggle: (c, on) => {
        const want = on === undefined ? !set.has(c) : on;
        if (want) set.add(c); else set.delete(c);
        return want;
      }
    },
    get textContent() { return text + el.children.map((c) => c.textContent).join(''); },
    set textContent(v) { text = String(v); el.children.forEach((c) => { c.parentNode = null; }); el.children = []; },
    get firstChild() { return el.children[0] || null; },
    appendChild(child) {
      if (child.parentNode) child.parentNode.removeChild(child);
      child.parentNode = el;
      el.children.push(child);
      return child;
    },
    removeChild(child) { el.children = el.children.filter((c) => c !== child); child.parentNode = null; return child; },
    remove() { if (el.parentNode) el.parentNode.removeChild(el); },
    set innerHTML(html) { el.children = []; text = ''; el._html = html; },
    get innerHTML() { return el._html || ''; },
    setAttribute(name, value) {
      const v = String(value);
      if (name.startsWith('data-')) el.dataset[datasetKey(name)] = v;
      else if (name === 'id') el.id = v;
      else if (name === 'class') el.className = v;
      else if (name === 'checked' || name === 'disabled' || name === 'selected') el[name] = true;
      else if (name === 'value') el.value = v;
      else if (name === 'type') el.type = v;
      else if (['placeholder', 'href', 'src', 'title', 'srcdoc'].includes(name)) el[name] = v;
      el._attrs[name] = v;
    },
    removeAttribute(name) { delete el._attrs[name]; },
    getAttribute(name) { return getAttr(el, name) ?? null; },
    addEventListener(type, fn) { (el._listeners[type] ||= []).push(fn); },
    removeEventListener(type, fn) { el._listeners[type] = (el._listeners[type] || []).filter((f) => f !== fn); },
    querySelector(sel) { return descendants(el).find((n) => matchesSelector(n, sel)) || null; },
    querySelectorAll(sel) { return descendants(el).filter((n) => matchesSelector(n, sel)); },
    closest(sel) {
      let node = el;
      while (node && node.tagName) { if (matchesSelector(node, sel)) return node; node = node.parentNode; }
      return null;
    },
    focus() {},
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 200, height: 20 }),
    setPointerCapture() {},
    get offsetWidth() { return 100; },
    dispatch(type, init = {}) {
      const event = { type, target: el, currentTarget: el, defaultPrevented: false, ...init,
                      preventDefault() { event.defaultPrevented = true; } };
      let node = el;
      while (node) {
        event.currentTarget = node;
        (node._listeners[type] || []).slice().forEach((fn) => fn(event));
        node = node.parentNode;
      }
      return event;
    },
    dispatchEvent(e) { return el.dispatch(e.type); }
  };
  if (tag === 'select') {
    Object.defineProperty(el, 'value', {
      get() { const s = el.children.find((o) => o.selected) || el.children[0]; return s ? s.value : ''; },
      set(v) { el.children.forEach((o) => { o.selected = o.value === String(v); }); }
    });
  }
  return el;
}

/** Dokument aus einer HTML-artigen Beschreibung: [{tag, id, cls, attrs, children}] */
export function makeDocument(spec = []) {
  const body = makeElement('body');
  const listeners = {};
  const build = (parent, nodes) => nodes.forEach((n) => {
    const e = makeElement(n.tag || 'div', n.cls || []);
    if (n.id) e.id = n.id;
    Object.entries(n.attrs || {}).forEach(([k, v]) => e.setAttribute(k, v));
    if (n.text) e.textContent = n.text;
    parent.appendChild(e);
    build(e, n.children || []);
  });
  build(body, spec);
  const doc = {
    body,
    visibilityState: 'visible',
    title: '',
    createElement: (tag) => makeElement(tag),
    createTextNode: (t) => { const n = makeElement('#text'); n.textContent = t; return n; },
    getElementById: (id) => descendants(body).find((n) => n.id === id) || null,
    querySelector: (sel) => body.querySelector(sel),
    querySelectorAll: (sel) => body.querySelectorAll(sel),
    addEventListener: (type, fn) => { (listeners[type] ||= []).push(fn); },
    fire: (type) => (listeners[type] || []).forEach((fn) => fn({ type }))
  };
  return doc;
}

/**
 * Skripte mit DOM, Fake-fetch und steuerbarer Uhr laden.
 * respond(url, init) → {status, body} (Objekt = JSON) oder Error
 */
export function loadPage(files, { spec = [], respond = () => ({ status: 200, body: {} }), clock = makeClock(),
                                  location = { pathname: '/', host: 'test', reload() {} }, extra = {} } = {}) {
  const document = makeDocument(spec);
  const requests = [];
  const fetch = (url, init = {}) => {
    requests.push({ url, method: init.method || 'GET', body: init.body });
    let r;
    try { r = respond(url, init); } catch (e) { return Promise.reject(e); }
    if (r instanceof Error) return Promise.reject(r);
    const status = r.status ?? 200;
    const isJson = typeof r.body === 'object';
    return Promise.resolve({
      ok: status >= 200 && status < 300,
      status,
      headers: { get: (h) => (h.toLowerCase() === 'content-type' ? (isJson ? 'application/json' : 'text/plain') : null) },
      json: () => Promise.resolve(r.body),
      text: () => Promise.resolve(isJson ? JSON.stringify(r.body) : String(r.body ?? '')),
      arrayBuffer: () => Promise.resolve(r.body)
    });
  };
  const ctx = {
    console: { log() {}, error() {}, warn() {} },
    Promise, JSON, Math, Number, String, Array, Object, Map, Set, RegExp, Error, DataView, Uint8Array,
    ArrayBuffer, isNaN, isFinite, parseInt, parseFloat, encodeURIComponent, decodeURIComponent, escape,
    Date: new Proxy(Date, { get: (t, p) => (p === 'now' ? clock.now : Reflect.get(t, p)) }),
    setTimeout: clock.setTimeout, clearTimeout: clock.clearTimeout,
    setInterval: (fn, ms) => { ctx._intervals.push({ fn, ms }); return ctx._intervals.length; }, clearInterval: () => {},
    performance: { now: clock.now },
    requestAnimationFrame: () => 0,
    Event: class { constructor(type) { this.type = type; } },
    FormData: class { constructor() { this.entries = []; } append(k, v, n) { this.entries.push([k, v, n]); } },
    document, fetch, location, confirm: () => true,
    AbortController: class { constructor() { this.signal = {}; } abort() {} },
    _intervals: [],
    ...extra
  };
  ctx.window = ctx;
  ctx.globalThis = ctx;
  ctx.addEventListener = () => {};
  vm.createContext(ctx);
  for (const f of files) vm.runInContext(fs.readFileSync(path.join(JS_DIR, f), 'utf8'), ctx, { filename: f });
  return { ctx, document, requests, clock, start: () => document.fire('DOMContentLoaded') };
}

/** Alle anstehenden Promises abarbeiten */
export async function settle(n = 20) {
  for (let i = 0; i < n; i++) await new Promise((r) => setImmediate(r));
}

const VOID = new Set(['input', 'img', 'link', 'meta', 'br', 'hr']);

/** Einfacher Parser für die eigenen, sauber geschriebenen HTML-Seiten */
export function parseHtmlInto(doc, html) {
  const bodyStart = html.indexOf('<body');
  let src = html.slice(html.indexOf('>', bodyStart) + 1, html.lastIndexOf('</body>'));
  src = src.replace(/<script[\s\S]*?<\/script>/g, '').replace(/<!--[\s\S]*?-->/g, '');
  const bodyTag = /<body([^>]*)>/.exec(html);
  if (bodyTag) {
    const m = /data-title="([^"]*)"/.exec(bodyTag[1]);
    if (m) doc.body.dataset.title = m[1];
  }
  const stack = [doc.body];
  const re = /<\/?([a-zA-Z0-9]+)((?:\s+[a-zA-Z0-9_:-]+(?:="[^"]*")?)*)\s*\/?>|([^<]+)/g;
  let m;
  while ((m = re.exec(src)) !== null) {
    const top = stack[stack.length - 1];
    if (m[3] !== undefined) {
      const t = m[3].replace(/\s+/g, ' ');
      if (t.trim()) {
        const n = makeElement('#text');
        n.textContent = t;
        top.appendChild(n);
      }
      continue;
    }
    const tag = m[1].toLowerCase();
    if (m[0].startsWith('</')) {
      while (stack.length > 1 && stack.pop().tagName !== tag.toUpperCase());
      continue;
    }
    const el = makeElement(tag);
    const attrRe = /([a-zA-Z0-9_:-]+)(?:="([^"]*)")?/g;
    let a;
    while ((a = attrRe.exec(m[2] || '')) !== null) el.setAttribute(a[1], a[2] === undefined ? '' : a[2]);
    if (tag === 'option' && el._attrs.value === undefined) el.value = '';
    top.appendChild(el);
    if (!VOID.has(tag) && !m[0].endsWith('/>')) stack.push(el);
  }
  // Text der option als value, falls kein value-Attribut
  doc.body.querySelectorAll('option').forEach((o) => { if (o._attrs.value === undefined) o.value = o.textContent.trim(); });
  return doc;
}

const WEB_DIR = path.resolve(JS_DIR, '..');

/** Echte Seite aus firmware/web laden, mit den Skripten aus ihren <script>-Tags */
export function loadRealPage(name, opts = {}) {
  const html = fs.readFileSync(path.join(WEB_DIR, name), 'utf8');
  const scripts = [...html.matchAll(/<script src="\/js\/([^"]+)"/g)].map((s) => s[1]);
  const p = loadPage(scripts, { ...opts, spec: [] });
  parseHtmlInto(p.document, html);
  return { ...p, scripts, html };
}

/** Alle IDs, die ein Skript per getElementById sucht */
export function idsUsedBy(file) {
  const src = fs.readFileSync(path.join(JS_DIR, file), 'utf8');
  return [...src.matchAll(/getElementById\('([^']+)'\)/g)].map((m) => m[1]);
}

const API_DIR = path.resolve(JS_DIR, '../../test/vectors/api');

/** Fixture aus dem C++-Vertragstest */
export function fixture(name) {
  return JSON.parse(fs.readFileSync(path.join(API_DIR, name), 'utf8'));
}

/**
 * Antwortgeber mit den echten Backend-Antworten. overrides: {url|pfad: fn|antwort}
 * Schreibende Anfragen beantwortet er mit {ok:true}.
 */
export function apiResponder(overrides = {}) {
  const map = {
    '/api/v1/system': 'system.json', '/getLatestValues': 'latest.json', '/api/v1/config': 'config.json',
    '/api/v1/sensors': 'sensors.json', '/api/v1/mail/status': 'mail_status.json',
    '/api/v1/mail/templates/boot': 'template_boot.json', '/api/v1/mail/templates/alive': 'template_alive.json',
    '/api/v1/mail/templates/warning': 'template_boot.json', '/api/v1/mail/templates/style': 'template_style.json',
    '/api/v1/chronik/info': 'chronik_info.json', '/api/v1/ota': 'ota_status.json', '/status': 'status.json'
  };
  return (url, init = {}) => {
    const p = url.split('?')[0];
    const method = (init.method || 'GET').toUpperCase();
    const o = overrides[`${method} ${p}`] ?? overrides[p];
    if (o !== undefined) return typeof o === 'function' ? o(url, init) : o;
    if (method !== 'GET') return { status: 200, body: { ok: true } };
    if (p === '/api/v1/logs/settings') return { body: fixture('logs_settings.json') };
    if (p.startsWith('/api/v1/logs')) return { body: fixture('logs.json') };
    if (p === '/api/v1/chronik/data') return { body: new Uint8Array(0).buffer };
    if (map[p]) return { body: fixture(map[p]) };
    return { status: 404, body: { ok: false, error: 'not_found', message: 'Nicht gefunden' } };
  };
}
