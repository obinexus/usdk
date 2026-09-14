import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startHostLocal } from '@usdk/host-local';
import { ConnectedSession } from '../src/index.mjs';

// A real @usdk/host-local server, driven only through ConnectedSession's
// public API (never fetch() directly) - this is the actual browser-side
// client exercised against the actual Connected-mode server, not a mock
// of either side.
const ORIGIN = 'http://127.0.0.1:8420';

async function withServerAndSession(fn) {
  const handle = await startHostLocal({ port: 0, allowedOrigins: [ORIGIN] });
  const session = new ConnectedSession({ hostUrl: handle.url, pairingSecret: handle.pairingSecret });
  try {
    await fn(session, handle);
  } finally {
    await handle.stop();
  }
}

test('binding-javascript: pair() connects and reports loaded capabilities', async () => {
  await withServerAndSession(async (session) => {
    assert.equal(session.connected, false);
    const { loadedCapabilities } = await session.pair();
    assert.equal(session.connected, true);
    assert.ok(loadedCapabilities.includes('driver-llm-fixture'));
  });
});

test('binding-javascript: pairing with the wrong secret throws', async () => {
  await withServerAndSession(async (session, handle) => {
    const wrong = new ConnectedSession({ hostUrl: handle.url, pairingSecret: 'not-the-secret' });
    await assert.rejects(() => wrong.pair());
  });
});

test('binding-javascript: sendText() before pair() throws with a clear message', async () => {
  await withServerAndSession(async (session) => {
    await assert.rejects(() => session.sendText('hi'), /connect\(\) must be awaited/);
  });
});

test('binding-javascript: a full text turn commits and returns approved text', async () => {
  await withServerAndSession(async (session) => {
    await session.pair();
    const result = await session.sendText('what is the status?');
    assert.equal(result.status, 'committed');
    assert.ok(result.text.startsWith('[fixture]'));
  });
});

test('binding-javascript: a robotics action proposes, commits, and executes', async () => {
  await withServerAndSession(async (session) => {
    await session.pair();
    const result = await session.proposeRoboticsAction({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 });
    assert.equal(result.status, 'committed');
    assert.equal(result.executionCompleted, true);
  });
});

test('binding-javascript: emergencyStop() and cancelTurn() round-trip without throwing', async () => {
  await withServerAndSession(async (session) => {
    await session.pair();
    await assert.doesNotReject(() => session.emergencyStop());
    await assert.doesNotReject(() => session.cancelTurn());
  });
});

test('binding-javascript: disconnect() ends the session so a subsequent call is refused', async () => {
  await withServerAndSession(async (session, handle) => {
    await session.pair();
    assert.equal(handle.sessionCount(), 1);
    await session.disconnect();
    assert.equal(handle.sessionCount(), 0);
    assert.equal(session.connected, false);
  });
});
