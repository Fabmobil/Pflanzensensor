/** DeviceWait: Neustart-Erkennung über bootId (neu) und Uptime (wie 2.x) */
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { load, makeClock } from './load.mjs';

function rig(responses) {
  const clock = makeClock(0);
  let i = 0;
  const fetch = () => {
    const r = responses[Math.min(i++, responses.length - 1)];
    if (r instanceof Error) return Promise.reject(r);
    return Promise.resolve({ ok: true, json: () => Promise.resolve(r) });
  };
  const { ctx } = load(['devicewait.js'], {
    clock,
    globals: { fetch, AbortController: class { constructor() { this.signal = {}; } abort() {} },
               document: undefined, location: { host: 'x', reload() {} } }
  });
  return { D: ctx.DeviceWait, clock };
}

async function run(D, clock, baseline) {
  let done = null;
  D.waitForDevice({ baseline, timeoutMs: 60000, intervalMs: 1000 }).then(r => { done = r; });
  for (let k = 0; k < 2000 && !done; k++) {
    await new Promise(r => setImmediate(r));
    clock.advance(500);
  }
  return done;
}

test('neue bootId gilt als Neustart, gleiche nicht', async () => {
  const base = { uptime: 100, bootId: 1, _at: 0 };
  let { D, clock } = rig([new Error('weg'), { uptime: 200, bootId: 2 }]);
  let res = await run(D, clock, base);
  assert.equal(res.ok, true);
  ({ D, clock } = rig([{ uptime: 101, bootId: 1 }]));
  res = await run(D, clock, base);
  assert.equal(res.ok, false);
});
