/**
 * Tests für data/js/devicewait.js - die Erkennung, ob das Gerät nach einem
 * Neustart wieder da ist.
 *
 * Warum diese Datei getestet wird: die Logik entscheidet, wann der Browser neu
 * lädt. Lädt sie zu früh, sieht der Nutzer eine Fehlerseite; lädt sie nie,
 * hängt die Oberfläche. Beides ist am Gerät mühsam zu reproduzieren, weil
 * jeder Durchlauf einen echten Neustart und knapp eine Minute kostet.
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { load, makeClock, baselineOf } from './helpers/load.mjs';

/** Antwortfolge: Liste von Werten, die getStatus() nacheinander liefert. */
function sequence(steps, clock, stepMs = 1000) {
  let i = 0;
  return () => {
    const s = steps[Math.min(i, steps.length - 1)];
    i++;
    clock.advance(stepMs);
    return typeof s === 'function' ? s() : s;
  };
}

const up = (uptime, inUpdateMode = false) => ({ uptime, inUpdateMode, version: '1' });

test('kein Neustart: meldet Timeout statt vorschnell neu zu laden', async () => {
  const clock = makeClock();
  let t = 100;
  const w = load({ clock, respond: () => { t += 1; clock.advance(1000); return up(t); } });
  const res = await w.DeviceWait.waitForDevice({
    baseline: baselineOf(clock, 100), timeoutMs: 10000, settleMs: 0
  });
  assert.equal(res.ok, false);
  assert.equal(res.reason, 'timeout');
});

test('Neustart wird erkannt, sobald die Uptime unter dem Erwartungswert liegt', async () => {
  const clock = makeClock();
  const steps = [up(100), up(101), null, null, null, up(3)];
  const w2 = load({ clock, respond: sequence(steps, clock) });
  const res = await w2.DeviceWait.waitForDevice({
    baseline: baselineOf(clock, 100), timeoutMs: 60000, settleMs: 0
  });
  assert.equal(res.ok, true);
  assert.equal(res.status.uptime, 3);
});

test('zweiter Neustart kurz nach dem ersten: Uptime kann HÖHER sein als der Basiswert', async () => {
  // Basiswert 12 s. Das Gerät ist 20 s weg und meldet sich mit uptime 15
  // zurück - höher als der Basiswert. Ein naives uptime < basiswert.uptime
  // (15 < 12) würde das verpassen und bis zum Timeout hängen.
  const clock = makeClock();
  const base = baselineOf(clock, 12);
  const steps = [null, null, null, null, up(15)];
  const w2 = load({ clock, respond: sequence(steps, clock, 5000) });
  const res = await w2.DeviceWait.waitForDevice({ baseline: base, timeoutMs: 120000, settleMs: 0 });
  assert.equal(res.ok, true, 'Neustart mit höherer Uptime muss erkannt werden');
  assert.ok(!(15 < 12), 'Gegenprobe: die naive Regel greift hier nicht');
});

test('Basiswert zählt ab der Messung, nicht ab Beginn des Wartens', async () => {
  // Beim Dateisystem-Update sichert setUpdateFlags() erst den Flash und
  // antwortet dabei fast eine Minute nicht. Der Basiswert ist dann längst
  // veraltet, wenn das Pollen beginnt. Wird der Abstand ab Beginn des Wartens
  // gerechnet, hält die Drift-Regel das Gerät faelschlich für nicht neu
  // gestartet.
  const clock = makeClock();
  const base = baselineOf(clock, 10);
  clock.advance(60000); // 60 s vergehen im auslösenden Request
  const w2 = load({ clock, respond: sequence([up(50, true)], clock, 1000) });
  const res = await w2.DeviceWait.waitForDevice({
    baseline: base, until: (s) => s.inUpdateMode, timeoutMs: 30000, settleMs: 0
  });
  assert.equal(res.ok, true);
});

test('ohne Basiswert genügt nicht jede Lücke - der Zustand muss stimmen', async () => {
  const clock = makeClock();
  const steps = [null, null, up(5, true), up(5, true), up(4, false)];
  const w2 = load({ clock, respond: sequence(steps, clock) });
  const phases = new Set();
  const res = await w2.DeviceWait.waitForDevice({
    until: (s) => !s.inUpdateMode, timeoutMs: 60000, settleMs: 0,
    onProgress: (p) => phases.add(p.phase)
  });
  assert.equal(res.ok, true);
  assert.equal(res.status.inUpdateMode, false);
  assert.ok(phases.has('unreachable'), 'Ausfall muss als unreachable gemeldet werden');
  assert.ok(phases.has('not-ready'), 'Update-Modus muss als not-ready gemeldet werden');
});

test('ohne Basiswert und ohne Ausfall wird nichts akzeptiert', async () => {
  const clock = makeClock();
  const w = load({ clock, respond: () => { clock.advance(1000); return up(50); } });
  const res = await w.DeviceWait.waitForDevice({ timeoutMs: 3000, settleMs: 0 });
  assert.equal(res.ok, false);
});

test('requireReboot:false wartet nur auf den Zustand', async () => {
  // Beim Wechsel in den Update-Modus ist inUpdateMode selbst der gesuchte
  // Zustand - ein Neustartnachweis wäre zu streng, weil das Gerät schon
  // während des auslösenden Requests neu gestartet sein kann.
  const clock = makeClock();
  const steps = [up(50), up(51), up(52, true)];
  const w2 = load({ clock, respond: sequence(steps, clock) });
  const res = await w2.DeviceWait.waitForDevice({
    requireReboot: false, until: (s) => s.inUpdateMode, timeoutMs: 30000, settleMs: 0
  });
  assert.equal(res.ok, true);
  assert.equal(res.status.inUpdateMode, true);
});

test('getStatus liefert null statt zu werfen und stempelt die Messung', async () => {
  const ok = load({ respond: () => up(7) });
  const s = await ok.DeviceWait.getStatus();
  assert.equal(s.uptime, 7);
  assert.equal(s._at, ok.clock.now, '_at muss den Messzeitpunkt tragen');

  const dead = load({ respond: () => null });
  assert.equal(await dead.DeviceWait.getStatus(), null);
});

test('phaseText liefert für jede Phase einen Text', () => {
  const w = load();
  for (const phase of ['settling', 'shutting-down', 'unreachable', 'not-ready', 'ready']) {
    const txt = w.DeviceWait.phaseText({ phase, elapsedMs: 12000, timeoutMs: 90000 });
    assert.ok(txt.length > 0, `Phase ${phase} braucht einen Text`);
  }
});

test('createOverlay baut ein Overlay und räumt es wieder ab', () => {
  const w = load();
  const ov = w.DeviceWait.createOverlay({ title: 'Titel', message: 'Meldung' });
  assert.equal(w.document._findByClass('devicewait-overlay').length, 1);
  ov.setDetail('Detailzeile');
  let geklickt = false;
  ov.setActions([{ label: 'Erneut prüfen', onClick: () => { geklickt = true; } }]);
  const actions = w.document._findByClass('devicewait-actions')[0];
  actions.children[0].click();
  assert.ok(geklickt, 'Schaltfläche muss ihren Handler auslösen');
  ov.close();
  assert.equal(w.document.body.children.length, 0, 'Overlay muss entfernt sein');
});

test('runReboot lädt erst neu, wenn das Gerät wieder da ist', async () => {
  const clock = makeClock();
  const steps = [up(80), null, null, up(4)];
  const w2 = load({ clock, respond: sequence(steps, clock) });
  let ausgeloest = false;
  await w2.DeviceWait.runReboot({
    trigger: () => { ausgeloest = true; },
    title: 'Neustart', message: '', timeoutMs: 60000
  });
  assert.ok(ausgeloest, 'Auslöser muss laufen');
  assert.equal(w2.reloads.length, 1, 'genau ein Reload, und zwar am Ende');
});

test('runReboot überlebt einen Auslöser, der wirft', async () => {
  // Das Gerät kann neu starten, bevor die Antwort auf den POST rausgeht -
  // ein abgerissener Auslöser ist deshalb kein Fehlerfall.
  const clock = makeClock();
  const steps = [up(80), null, up(4)];
  const w2 = load({ clock, respond: sequence(steps, clock) });
  await w2.DeviceWait.runReboot({
    trigger: () => { throw new Error('Verbindung abgerissen'); },
    title: 'Neustart', message: '', timeoutMs: 60000
  });
  assert.equal(w2.reloads.length, 1, 'trotz Fehler beim Auslöser normal weiterwarten');
});

test('runReboot zeigt bei Ablauf der Frist Handlungsoptionen statt endlos zu drehen', async () => {
  const clock = makeClock();
  const w = load({ clock, respond: () => { clock.advance(500); return null; } });
  await w.DeviceWait.runReboot({
    trigger: () => {}, title: 'Neustart', message: '', timeoutMs: 2000
  });
  assert.equal(w.reloads.length, 0, 'kein Reload, wenn das Gerät wegbleibt');
  const actions = w.document._findByClass('devicewait-actions')[0];
  assert.equal(actions.children.length, 2, 'Erneut prüfen + Trotzdem neu laden');
});
