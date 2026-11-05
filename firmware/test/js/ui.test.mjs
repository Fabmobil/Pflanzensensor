/** Gemeinsame UI-Bausteine, Schwellenregler, Startseite, Logs, Update */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { load, makeClock } from './load.mjs';

test('Statustexte aus einer Tabelle (B-52) und Formatierer', () => {
  const { ctx } = load(['ui.js']);
  assert.equal(ctx.UI.statusText('yellow'), 'Warnung');
  assert.equal(ctx.UI.statusText('kaputt'), 'Unbekannt');
  assert.equal(ctx.UI.formatUptime(90061), '1d 1h 1m');
  assert.equal(ctx.UI.formatValue(null, '%'), '--');
  assert.equal(ctx.UI.formatValue(21.36, '°C'), '21.4°C');
  assert.deepEqual(JSON.parse(JSON.stringify(ctx.UI.patchFor('display.showQr', true))), { display: { showQr: true } });
});

test('Autosave: Debounce je Feld - zwei Felder gehen beide durch (B-50)', async () => {
  const clock = makeClock();
  const { ctx } = load(['ui.js'], { clock });
  const field = () => {
    const l = {};
    return { type: 'text', tagName: 'INPUT', value: '', classList: { add() {}, remove() {} },
             addEventListener: (e, f) => { l[e] = f; }, fire: () => l.input() };
  };
  const a = field(), b = field();
  const saved = [];
  ctx.UI.bindAutosave(a, v => saved.push(['a', v]), { silent: true });
  ctx.UI.bindAutosave(b, v => saved.push(['b', v]), { silent: true });
  a.value = '1'; a.fire();
  clock.advance(300);
  b.value = '2'; b.fire();
  clock.advance(1500);
  await Promise.resolve();
  assert.deepEqual(saved, [['a', '1'], ['b', '2']]);
});

test('Schwellenregler: Bereich nach Einheit (B-58), einseitig und gleiche Werte (B-57)', () => {
  const { ctx } = load(['ui.js', 'threshold-slider.js']);
  const S = ctx.ThresholdSlider;
  assert.deepEqual(Array.from(S.calculateRange({ unit: '%', yellowLow: 10, greenLow: 20, greenHigh: 80, yellowHigh: 90 })), [0, 100]);
  const r = S.calculateRange({ unit: 'hPa', yellowLow: 950, greenLow: 980, greenHigh: 1030, yellowHigh: 1060, validMin: 300, validMax: 1100 });
  assert.ok(r[0] < 950 && r[0] > 900 && r[1] > 1060 && r[1] <= 1100);
  const eq = S.calculateRange({ unit: 'x', yellowLow: 5, greenLow: 5, greenHigh: 5, yellowHigh: 5 });
  assert.ok(eq[1] > eq[0]);
  const lim = { yellowLow: 10, greenLow: 20, greenHigh: 20, yellowHigh: 90 };
  assert.equal(S.clampOrdered(lim, 'greenLow', 50, false).greenLow, 20);
  assert.equal(S.clampOrdered(lim, 'greenHigh', 5, false).greenHigh, 20);
  assert.equal(S.clampOrdered({ yellowLow: 0, greenLow: 0, greenHigh: 25, yellowHigh: 50 }, 'greenHigh', 40, true).greenHigh, 40);
  assert.match(S.gradient({ greenHigh: 25, yellowHigh: 50 }, [0, 100], true), /#5ac85a 0%/);
});

test('Schwellenregler: Skala, Beschriftungen und Live-Werte', () => {
  const { ctx } = load(['ui.js', 'threshold-slider.js']);
  const S = ctx.ThresholdSlider;
  assert.deepEqual(Array.from(S.ticks([0, 100])), [0, 20, 40, 60, 80, 100]);
  assert.deepEqual(Array.from(S.ticks([7.3, 31.8])), [10, 15, 20, 25, 30]);
  assert.ok(S.ticks([0, 1]).every((v) => v >= 0 && v <= 1));
  // nahe Beschriftungen wandern in eine zweite Reihe, entfernte bleiben unten
  assert.deepEqual(Array.from(S.stackRows([{ pos: 10, width: 30 }, { pos: 20, width: 30 }, { pos: 100, width: 30 }], 4)), [0, 1, 0]);
});

test('Startseite: Gesicht und Countdown', () => {
  const { ctx } = load(['ui.js', 'start.js']);
  assert.equal(ctx.Start.faceFor('green'), '/img/face-happy.gif');
  assert.equal(ctx.Start.faceFor('red'), '/img/face-sad.gif');
  assert.equal(ctx.Start.faceFor('warmup'), '/img/face-error.gif');
  assert.equal(ctx.Start.intervalText(10000, 60000, 25500), '(15s/60s)');
  assert.equal(ctx.Start.intervalText(0, 60000, 1), '(--/--)');
  assert.deepEqual(Array.from(ctx.Start.FAST_POLL_DELAYS_MS), [1500, 2000, 2500, 3000, 4000, 5000]);
});

test('Logs: höchstens 100 Einträge', () => {
  const { ctx } = load(['ui.js', 'logs.js']);
  const out = ctx.Logs.merge(Array.from({ length: 95 }, (_, i) => i), Array.from({ length: 10 }, (_, i) => 100 + i), 100);
  assert.equal(out.length, 100);
  assert.equal(out[99], 109);
  assert.equal(out[0], 5);
});

test('Update: MD5-Prüfung und Upload-URL', () => {
  const { ctx } = load(['ui.js', 'update.js']);
  assert.ok(ctx.Update.validMd5('0123456789abcdef0123456789ABCDEF'));
  assert.ok(!ctx.Update.validMd5('xyz'));
  assert.equal(ctx.Update.uploadUrl({ size: 1234 }, 'ab'), '/api/v1/ota?size=1234&md5=ab');
});

test('Mail: leere Auswahl heißt alle', () => {
  const { ctx } = load(['ui.js', 'mail.js']);
  assert.ok(ctx.Mail.isWatched('', 'DHT_1'));
  assert.ok(ctx.Mail.isWatched('ANALOG_1, DHT_1', 'DHT_1'));
  assert.ok(!ctx.Mail.isWatched('DHT_10', 'DHT_1'));
});

test('Admin: Passwortregeln wie am Gerät (E10, B-49)', () => {
  const { ctx } = load(['ui.js', 'admin.js']);
  assert.equal(ctx.Admin.passwordProblem('x', 'x'), null);
  assert.ok(ctx.Admin.passwordProblem('', ''));
  assert.ok(ctx.Admin.passwordProblem('ä', 'ä'));
  assert.ok(ctx.Admin.passwordProblem('a'.repeat(33), 'a'.repeat(33)));
  assert.ok(ctx.Admin.passwordProblem('a', 'b'));
});

test('Mailtexte: Zähler gelb ab 90 %, rot über 100 %', () => {
  const { ctx } = load(['ui.js', 'template-engine.js', 'mail-templates.js']);
  assert.equal(ctx.MailTemplates.counterClass(100, 1600), 'counter');
  assert.equal(ctx.MailTemplates.counterClass(1500, 1600), 'counter warn');
  assert.equal(ctx.MailTemplates.counterClass(1601, 1600), 'counter over');
  assert.equal(ctx.MailTemplates.EMOJIS.length, 36);
});
