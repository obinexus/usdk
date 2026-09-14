import { UsdkError, Status } from '@usdk/contracts';

/** Shared bounds every robotics action is checked against, at proposal
 * time (before a candidate is even created - see @usdk/host-browser)
 * AND again at execution time inside the driver (defense in depth - a
 * candidate approved a few seconds ago should not bypass a fresh bounds
 * check). Deliberately conservative for a simulated/demo driver. */
export const ROBOTICS_LIMITS = Object.freeze({
  maxDistanceM: 1.0,
  maxSpeedMS: 0.5,
  allowedDirections: Object.freeze(['forward', 'backward', 'left', 'right']),
});

/**
 * @typedef {Object} RoboticsAction
 * @property {string} direction - one of ROBOTICS_LIMITS.allowedDirections
 * @property {number} distanceM
 * @property {number} speedMS
 */

/** Throws UsdkError(INVALID_ARGUMENT) if `action` is not a well-formed,
 * in-bounds structured movement. Never accepts free-form text as an
 * action - see docs/UAGENT_ARCHITECTURE.md "Robotics": "Do not let
 * spoken content directly become executable robot commands."
 * @param {unknown} action */
export function validateRoboticsAction(action) {
  const fail = (msg) => { throw new UsdkError(Status.INVALID_ARGUMENT, msg); };
  if (!action || typeof action !== 'object') fail('robotics action must be a structured object');
  const a = /** @type {any} */ (action);
  if (!ROBOTICS_LIMITS.allowedDirections.includes(a.direction)) {
    fail(`direction '${a.direction}' is not one of ${ROBOTICS_LIMITS.allowedDirections.join(', ')}`);
  }
  if (typeof a.distanceM !== 'number' || !(a.distanceM > 0) || a.distanceM > ROBOTICS_LIMITS.maxDistanceM) {
    fail(`distanceM must be in (0, ${ROBOTICS_LIMITS.maxDistanceM}]`);
  }
  if (typeof a.speedMS !== 'number' || !(a.speedMS > 0) || a.speedMS > ROBOTICS_LIMITS.maxSpeedMS) {
    fail(`speedMS must be in (0, ${ROBOTICS_LIMITS.maxSpeedMS}]`);
  }
  return { direction: a.direction, distanceM: a.distanceM, speedMS: a.speedMS };
}

/**
 * A conforming driver instance must implement:
 *   execute(action: RoboticsAction, turnId: string): Promise<{completed: boolean}>
 *   emergencyStop(): void - bypasses any in-flight deliberation/wait and
 *     cancels pending movement immediately, per this task's own
 *     instruction.
 *   isAvailable(): boolean
 *   label(): string
 */
export function checkRoboticsCapability(instance) {
  const missing = [];
  for (const m of ['execute', 'emergencyStop', 'isAvailable', 'label']) {
    if (typeof instance?.[m] !== 'function') missing.push(`${m}()`);
  }
  return missing;
}

export function assertRoboticsCapability(instance) {
  const missing = checkRoboticsCapability(instance);
  if (missing.length > 0) {
    throw new UsdkError(Status.INVALID_ARGUMENT, `object does not implement capability-robotics: missing ${missing.join(', ')}`);
  }
  return instance;
}
