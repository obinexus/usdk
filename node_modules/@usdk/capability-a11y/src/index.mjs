import { UsdkError, Status } from '@usdk/contracts';

/**
 * The capability-a11y contract. A conforming implementation must
 * provide:
 *   announce(text: string, opts?: {assertive?: boolean}): void - posts
 *     to a live region (see @usdk/uagent's use of aria-live)
 *   setStatus(text: string): void - a persistent, readable status line
 *     (recording/processing/speaking/idle), not just a transient
 *     announcement
 *   isAvailable(): true - always, by construction; there is no
 *     "unavailable" state for this capability (see package.json).
 */

export function checkA11yCapability(instance) {
  const missing = [];
  for (const m of ['announce', 'setStatus', 'isAvailable']) {
    if (typeof instance?.[m] !== 'function') missing.push(`${m}()`);
  }
  return missing;
}

export function assertA11yCapability(instance) {
  const missing = checkA11yCapability(instance);
  if (missing.length > 0) {
    throw new UsdkError(Status.INVALID_ARGUMENT, `object does not implement capability-a11y: missing ${missing.join(', ')}`);
  }
  return instance;
}
