import { UsdkError, Status } from '@usdk/contracts';

export const descriptor = { name: 'driver-vision', version: '0.1', kind: 'vision' };

/**
 * No offline/browser-compatible vision model was confirmed available in
 * this environment - this driver honestly reports itself unavailable
 * rather than fabricating a description. See
 * docs/IMPLEMENTATION_STATUS.md for what a real driver would need.
 */
export function create() {
  return Object.freeze({
    async describe() {
      throw new UsdkError(Status.NOT_IMPLEMENTED, 'no vision capability is available - see @usdk/driver-vision README');
    },
    isAvailable() {
      return false;
    },
    label() {
      return '[vision unavailable - no offline/browser-compatible vision model in this environment]';
    },
  });
}
