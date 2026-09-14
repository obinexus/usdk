import { validateRoboticsAction } from '@usdk/capability-robotics';

export const descriptor = { name: 'driver-robotics-sim', version: '0.1', kind: 'robotics' };

/**
 * @param {Object} [config]
 * @param {(action: import('@usdk/capability-robotics').RoboticsAction) => Promise<void>} [config.onMove]
 *   called with each "tick" of simulated movement - e.g. to animate a
 *   position in the UI. Optional; defaults to a no-op.
 * @param {number} [config.stepMs] simulated time per action, default 300
 */
export function create(config = {}) {
  const onMove = config.onMove ?? (async () => {});
  const stepMs = config.stepMs ?? 300;

  let stopped = false;
  /** @type {{resolve: (v: {completed: boolean}) => void} | null} */
  let inFlight = null;

  return Object.freeze({
    /**
     * Re-validates `action` against the shared bounds (defense in
     * depth - a candidate approved a few seconds ago should not bypass
     * a fresh check) and simulates movement in `stepMs`-sized
     * increments, checking `stopped` between each so emergencyStop()
     * takes effect promptly rather than only before/after the whole
     * action.
     * @param {unknown} action @param {string} turnId
     * @returns {Promise<{completed: boolean}>}
     */
    async execute(action, turnId) {
      const validated = validateRoboticsAction(action);
      if (stopped) return { completed: false };

      const steps = 4;
      for (let i = 0; i < steps; i++) {
        if (stopped) return { completed: false };
        await new Promise((resolve) => {
          inFlight = { resolve: () => resolve(undefined) };
          setTimeout(() => resolve(undefined), stepMs / steps);
        });
        inFlight = null;
        if (stopped) return { completed: false };
        await onMove({ ...validated, step: i + 1, of: steps, turnId });
      }
      return { completed: true };
    },

    /** Bypasses deliberation entirely (a caller wires this to a UI
     * control that does NOT wait for the three-party vote - see
     * docs/UAGENT_ARCHITECTURE.md "Robotics") and cancels any
     * in-progress simulated movement immediately. */
    emergencyStop() {
      stopped = true;
      if (inFlight) {
        inFlight.resolve();
        inFlight = null;
      }
    },

    /** Clears the stopped latch so a fresh session can move again -
     * distinct from construction so a caller can require an explicit
     * "resume" action rather than silently un-sticking e-stop. */
    reset() {
      stopped = false;
    },

    isAvailable() {
      return true;
    },
    label() {
      return '[simulated robotics - no physical hardware]';
    },
  });
}

export { validateRoboticsAction, ROBOTICS_LIMITS } from '@usdk/capability-robotics';
