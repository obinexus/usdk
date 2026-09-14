import { test } from 'node:test';
import assert from 'node:assert/strict';
import { UsdkError } from '@usdk/contracts';
import { assertRoboticsCapability } from '@usdk/capability-robotics';
import { create } from '../src/index.mjs';

test('driver-robotics-sim conforms to capability-robotics', () => {
  assertRoboticsCapability(create());
});

test('a valid, in-bounds action completes', async () => {
  const d = create({ stepMs: 20 });
  const result = await d.execute({ direction: 'forward', distanceM: 0.3, speedMS: 0.1 }, 'turn-1');
  assert.equal(result.completed, true);
});

test('an invalid action is rejected before any movement happens (revalidated at execution time)', async () => {
  const d = create({ stepMs: 20 });
  await assert.rejects(
    () => d.execute({ direction: 'diagonal', distanceM: 0.3, speedMS: 0.1 }, 'turn-1'),
    UsdkError
  );
});

test('emergencyStop cancels a movement already in progress', async () => {
  const moves = [];
  const d = create({ stepMs: 200, onMove: async (m) => { moves.push(m); } });
  const execPromise = d.execute({ direction: 'forward', distanceM: 1.0, speedMS: 0.5 }, 'turn-1');
  // Let the first step or two start, then stop mid-flight.
  await new Promise((r) => setTimeout(r, 120));
  d.emergencyStop();
  const result = await execPromise;
  assert.equal(result.completed, false);
  assert.ok(moves.length < 4, `expected fewer than all 4 steps to complete, got ${moves.length}`);
});

test('after emergencyStop, a new execute() call also refuses to move until reset()', async () => {
  const d = create({ stepMs: 10 });
  d.emergencyStop();
  const result = await d.execute({ direction: 'forward', distanceM: 0.1, speedMS: 0.1 }, 'turn-2');
  assert.equal(result.completed, false);
  d.reset();
  const result2 = await d.execute({ direction: 'forward', distanceM: 0.1, speedMS: 0.1 }, 'turn-3');
  assert.equal(result2.completed, true);
});
