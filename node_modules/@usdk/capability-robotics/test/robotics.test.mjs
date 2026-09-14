import { test } from 'node:test';
import assert from 'node:assert/strict';
import { UsdkError } from '@usdk/contracts';
import { validateRoboticsAction, ROBOTICS_LIMITS, assertRoboticsCapability } from '../src/index.mjs';

test('validateRoboticsAction accepts an in-bounds action', () => {
  const a = validateRoboticsAction({ direction: 'forward', distanceM: 0.5, speedMS: 0.2 });
  assert.equal(a.direction, 'forward');
});

test('validateRoboticsAction rejects an unknown direction', () => {
  assert.throws(() => validateRoboticsAction({ direction: 'sideways', distanceM: 0.1, speedMS: 0.1 }), UsdkError);
});

test('validateRoboticsAction rejects a distance beyond the bound', () => {
  assert.throws(
    () => validateRoboticsAction({ direction: 'forward', distanceM: ROBOTICS_LIMITS.maxDistanceM + 1, speedMS: 0.1 }),
    UsdkError
  );
});

test('validateRoboticsAction rejects free-form text instead of a structured action', () => {
  assert.throws(() => validateRoboticsAction('move forward please'), UsdkError);
  assert.throws(() => validateRoboticsAction(null), UsdkError);
});

test('assertRoboticsCapability requires emergencyStop specifically', () => {
  assert.throws(
    () => assertRoboticsCapability({ execute: async () => {}, isAvailable: () => true, label: () => 'x' }),
    (e) => e instanceof UsdkError && /emergencyStop/.test(e.message)
  );
});
