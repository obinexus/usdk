import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ConversationSession } from '../src/index.mjs';

// baseUrl for manifest `entry` resolution - points at the REAL sibling
// driver packages' actual source, not synthetic test fixtures, so this
// test exercises the real @usdk/driver-llm-fixture and
// @usdk/driver-robotics-sim through the genuine dynamic-import path
// (@usdk/loader), the same path a browser would use.
const baseUrl = import.meta.url;

function fakeA11y() {
  const announcements = [];
  const statuses = [];
  return {
    announce: (text) => announcements.push(text),
    setStatus: (text) => statuses.push(text),
    isAvailable: () => true,
    _announcements: announcements,
    _statuses: statuses,
  };
}

const manifests = [
  { name: 'driver-llm-fixture', version: '0.1', kind: 'llm', entry: '../../driver-llm-fixture/src/index.mjs', dependencies: [] },
  { name: 'driver-robotics-sim', version: '0.1', kind: 'robotics', entry: '../../driver-robotics-sim/src/index.mjs', dependencies: [] },
];

test('host-browser: a text turn with sufficient evidence and a permitted constraint commits and is announced', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's1',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    a11y,
    speakApprovedText: false,
  });
  const { loadedCapabilities } = await session.init();
  assert.ok(loadedCapabilities.includes('driver-llm-fixture'));

  const result = await session.sendText('what is the status?');
  assert.equal(result.status, 'committed');
  assert.ok(result.text.startsWith('[fixture]'));
  assert.ok(a11y._announcements.some((a) => a === result.text));
});

test('host-browser: a disallowed constraint is rejected (insufficient permission)', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's2',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    a11y,
    allowedConstraints: [], // nothing permitted, including the default text constraint
  });
  await session.init();
  const result = await session.sendText('hello');
  assert.equal(result.status, 'rejected');
});

test('host-browser: an already-expired round deadline times out', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's3',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    a11y,
    roundTimeoutMs: -1000, // deadline already in the past by the time the round opens
  });
  await session.init();
  const result = await session.sendText('hello');
  assert.equal(result.status, 'timed-out');
});

test('host-browser: a permitted robotics action executes through the dispatch gate', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's4',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    roboticsManifestName: 'driver-robotics-sim',
    a11y,
    allowedConstraints: ['robotics:bounded-move'],
    maxUncertainty: 0.5,
  });
  await session.init();
  const result = await session.proposeRoboticsAction({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 });
  assert.equal(result.status, 'committed');
  assert.equal(result.executionCompleted, true);
});

test('host-browser: a committed robotics action interrupted by emergencyStop reports committed but not completed - found via a real browser session that showed a misleading "executed" message before this fix', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's4b',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    roboticsManifestName: 'driver-robotics-sim',
    a11y,
    allowedConstraints: ['robotics:bounded-move'],
    maxUncertainty: 0.5,
  });
  await session.init();
  const resultPromise = session.proposeRoboticsAction({ direction: 'forward', distanceM: 1.0, speedMS: 0.5 });
  // The fixture driver approves instantly; the simulated movement itself
  // takes ~300ms (4 steps) by default - stop partway through it.
  await new Promise((r) => setTimeout(r, 100));
  session.emergencyStop();
  const result = await resultPromise;
  assert.equal(result.status, 'committed'); // the PROPOSAL was still approved
  assert.equal(result.executionCompleted, false); // but it did not finish
});

test('host-browser: a robotics action outside the permission policy never executes', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's5',
    manifests,
    baseUrl,
    llmManifestName: 'driver-llm-fixture',
    roboticsManifestName: 'driver-robotics-sim',
    a11y,
    allowedConstraints: [], // robotics:bounded-move not permitted
  });
  await session.init();
  const result = await session.proposeRoboticsAction({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 });
  assert.equal(result.status, 'rejected');
});

test('host-browser: a malformed robotics action is rejected before any candidate/round exists', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's6', manifests, baseUrl, llmManifestName: 'driver-llm-fixture', roboticsManifestName: 'driver-robotics-sim', a11y,
    allowedConstraints: ['robotics:bounded-move'],
  });
  await session.init();
  await assert.rejects(() => session.proposeRoboticsAction({ direction: 'sideways', distanceM: 99, speedMS: 99 }));
});

test('host-browser: cancelTurn() causes an in-flight turn to report superseded, not committed', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's7', manifests, baseUrl, llmManifestName: 'driver-llm-fixture', a11y,
  });
  await session.init();
  const p = session.sendText('a question');
  session.cancelTurn();
  const result = await p;
  assert.equal(result.status, 'superseded');
});

test('host-browser: cancelTurn() during an in-flight robotics proposal reports superseded, not committed, and never dispatches', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's7b', manifests, baseUrl, llmManifestName: 'driver-llm-fixture', roboticsManifestName: 'driver-robotics-sim', a11y,
    allowedConstraints: ['robotics:bounded-move'],
  });
  await session.init();
  const p = session.proposeRoboticsAction({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 });
  session.cancelTurn();
  const result = await p;
  assert.equal(result.status, 'superseded');
  assert.equal(result.executionCompleted, undefined); // never reached dispatch, so this field is never set
});

test('host-browser: emergencyStop() reaches the robotics driver directly, bypassing deliberation', async () => {
  const a11y = fakeA11y();
  const session = new ConversationSession({
    sessionId: 's8', manifests, baseUrl, llmManifestName: 'driver-llm-fixture', roboticsManifestName: 'driver-robotics-sim', a11y,
    allowedConstraints: ['robotics:bounded-move'],
  });
  await session.init();
  // Should not throw even with no round in flight - it is a direct call
  // to the driver, not gated by any round/candidate state.
  assert.doesNotThrow(() => session.emergencyStop());
});
