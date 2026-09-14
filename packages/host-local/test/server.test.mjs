import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startHostLocal } from '../src/index.mjs';

const ORIGIN = 'http://127.0.0.1:8420';

async function withServer(fn) {
  // port:0 asks the OS for a free port - handle.url already reflects it.
  const handle = await startHostLocal({ port: 0, allowedOrigins: [ORIGIN] });
  try {
    await fn(handle);
  } finally {
    await handle.stop();
  }
}

function withOrigin(headers = {}) {
  return { Origin: ORIGIN, ...headers };
}

test('host-local: a request from a disallowed origin is refused', async () => {
  await withServer(async (handle) => {
    const res = await fetch(`${handle.url}/session`, {
      method: 'POST',
      headers: withOrigin({ Origin: 'http://evil.example', 'X-Usdk-Pairing-Secret': handle.pairingSecret }),
    });
    assert.equal(res.status, 403);
  });
});

test('host-local: creating a session without the pairing secret is refused', async () => {
  await withServer(async (handle) => {
    const res = await fetch(`${handle.url}/session`, { method: 'POST', headers: withOrigin() });
    assert.equal(res.status, 401);
  });
});

test('host-local: full flow - pair, create session, send a text turn, get committed result', async () => {
  await withServer(async (handle) => {
    const createRes = await fetch(`${handle.url}/session`, {
      method: 'POST',
      headers: withOrigin({ 'X-Usdk-Pairing-Secret': handle.pairingSecret }),
    });
    assert.equal(createRes.status, 200);
    const { sessionId, token, loadedCapabilities } = await createRes.json();
    assert.ok(loadedCapabilities.includes('driver-llm-fixture'));
    assert.equal(handle.sessionCount(), 1);

    const turnRes = await fetch(`${handle.url}/session/${sessionId}/turn`, {
      method: 'POST',
      headers: withOrigin({ Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' }),
      body: JSON.stringify({ text: 'what is the status?' }),
    });
    assert.equal(turnRes.status, 200);
    const result = await turnRes.json();
    assert.equal(result.status, 'committed');
    assert.ok(result.text.startsWith('[fixture]'));
    assert.equal(result.a11y.announcements[0], result.text);
  });
});

test('host-local: a session request with a wrong bearer token is refused', async () => {
  await withServer(async (handle) => {
    const createRes = await fetch(`${handle.url}/session`, {
      method: 'POST',
      headers: withOrigin({ 'X-Usdk-Pairing-Secret': handle.pairingSecret }),
    });
    const { sessionId } = await createRes.json();
    const res = await fetch(`${handle.url}/session/${sessionId}/turn`, {
      method: 'POST',
      headers: withOrigin({ Authorization: 'Bearer not-the-real-token', 'Content-Type': 'application/json' }),
      body: JSON.stringify({ text: 'hi' }),
    });
    assert.equal(res.status, 401);
  });
});

test('host-local: a permitted robotics action executes end to end over HTTP', async () => {
  await withServer(async (handle) => {
    const createRes = await fetch(`${handle.url}/session`, {
      method: 'POST',
      headers: withOrigin({ 'X-Usdk-Pairing-Secret': handle.pairingSecret }),
    });
    const { sessionId, token } = await createRes.json();
    const res = await fetch(`${handle.url}/session/${sessionId}/robotics`, {
      method: 'POST',
      headers: withOrigin({ Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' }),
      body: JSON.stringify({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 }),
    });
    assert.equal(res.status, 200);
    const result = await res.json();
    assert.equal(result.status, 'committed');
    assert.equal(result.executionCompleted, true);
  });
});

test('host-local: estop is reachable over HTTP and reports ok', async () => {
  await withServer(async (handle) => {
    const createRes = await fetch(`${handle.url}/session`, {
      method: 'POST',
      headers: withOrigin({ 'X-Usdk-Pairing-Secret': handle.pairingSecret }),
    });
    const { sessionId, token } = await createRes.json();
    const res = await fetch(`${handle.url}/session/${sessionId}/estop`, {
      method: 'POST',
      headers: withOrigin({ Authorization: `Bearer ${token}` }),
    });
    assert.equal(res.status, 200);
    assert.deepEqual(await res.json(), { protocolVersion: 1, ok: true });
  });
});

test('host-local: /native/inspect rejects a module name not in the real manifest allowlist', async () => {
  await withServer(async (handle) => {
    const res = await fetch(
      `${handle.url}/native/inspect/../../etc/passwd?pairing=${encodeURIComponent(handle.pairingSecret)}`,
      { headers: withOrigin() }
    );
    // Either the URL parser normalizes the traversal away (404 route) or
    // the allowlist check rejects it (404 unknown module) - either way it
    // must never reach execFile with an unvalidated argument.
    assert.equal(res.status, 404);
  });
});

test('host-local: /native/doctor requires the pairing secret even though it is read-only', async () => {
  await withServer(async (handle) => {
    const res = await fetch(`${handle.url}/native/doctor`, { headers: withOrigin() });
    assert.equal(res.status, 401);
  });
});

test('host-local: /native/doctor, with the correct pairing secret, really spawns usdk.exe and returns its JSON', async () => {
  await withServer(async (handle) => {
    const res = await fetch(`${handle.url}/native/doctor?pairing=${encodeURIComponent(handle.pairingSecret)}`, { headers: withOrigin() });
    assert.equal(res.status, 200);
    const body = await res.json();
    assert.equal(body.doctor.overall, 'ok');
    assert.ok(body.doctor.checks.some((c) => c.name === 'role_modules'));
  });
});

test('host-local: /native/inspect/usdk-perceive, with the correct pairing secret, really spawns usdk.exe inspect', async () => {
  await withServer(async (handle) => {
    const res = await fetch(
      `${handle.url}/native/inspect/usdk-perceive?pairing=${encodeURIComponent(handle.pairingSecret)}`,
      { headers: withOrigin() }
    );
    assert.equal(res.status, 200);
    const body = await res.json();
    assert.equal(body.inspect.found, true);
    assert.equal(body.inspect.role, 'perceive');
  });
});
