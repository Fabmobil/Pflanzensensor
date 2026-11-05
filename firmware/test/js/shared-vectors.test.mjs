/**
 * Gemeinsame Testvektoren C++/JS: Mailvorlagen, Chronik-Rahmen, Status und
 * Hostname. Dieselben Dateien prüfen die nativen C++-Tests.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { load, vector } from './load.mjs';

test('Mailvorlagen: Zeilen wie am Gerät', () => {
  const { ctx } = load(['template-engine.js']);
  const T = ctx.TemplateEngine;
  const v = vector('mail_template.json');
  const block = n => [1, 2, 3].map(i => `<tr>${n} ${i}</tr>`);
  const bloecke = { messwerte: block('messwerte'), auffaellige: block('auffaellige') };
  for (const c of v.cases) {
    assert.deepEqual(Array.from(T.expandiereZeile(c.input, v.values, bloecke)), c.output, c.input.slice(0, 40));
  }
  for (const s of v.subjects) {
    assert.equal(T.expandiereBetreff(s.input, v.values), s.output);
  }
});

test('Chronik v2: Tabelle und Messrahmen wie am Gerät', () => {
  const { ctx } = load(['ui.js', 'chronik.js'], { globals: { addEventListener: () => {}, document: undefined } });
  const C = ctx.Chronik;
  const v = vector('chronik_frames.json');
  const hex = h => new Uint8Array(h.match(/../g).map(x => parseInt(x, 16)));
  const bytes = new Uint8Array([...hex(v.table), ...hex(v.sample)]);
  const r = C.parseStream(bytes);
  assert.equal(r.verworfen, 0);
  v.tableEntries.forEach(e => {
    const t = r.tabelle.get(e.channel);
    assert.equal(t.schluessel, e.key);
    assert.equal(t.name, e.name);
    assert.equal(t.einheit, e.unit);
    assert.equal(t.einseitig, e.oneSided);
    assert.equal(t.encoding.kind, e.encoding);
  });
  v.sampleDecoded.values.forEach(e => {
    const reihe = r.reihen.get(e.channel);
    assert.equal(reihe.t[0], v.sampleDecoded.epoch);
    if (e.value === null) assert.ok(Number.isNaN(reihe.v[0]));
    else assert.ok(Math.abs(reihe.v[0] - e.value) < 0.01, `${reihe.v[0]} ≈ ${e.value}`);
    assert.equal(reihe.roh[0] === null ? -1 : reihe.roh[0], e.raw);
  });
  for (const h of v.half) {
    const back = C.halfToFloat(h.bits);
    if (h.back === null) assert.ok(!Number.isFinite(back));
    else assert.ok(Math.abs(back - h.back) < 1e-3 * Math.max(1, Math.abs(h.back)));
  }
  // Messrahmen ohne Tabelle wird übersprungen, nicht falsch gedeutet
  const ohne = C.parseStream(hex(v.sample));
  assert.equal(ohne.reihen.size, 0);
  assert.equal(ohne.verworfen, 1);
});

test('Hostname-Vorschau wie MdnsName', () => {
  const { ctx } = load(['ui.js', 'admin.js'], { globals: { document: undefined } });
  for (const h of vector('status_hostname.json').hostnames) {
    assert.equal(ctx.Admin.hostnameFrom(h.name), h.host, h.name);
  }
});
