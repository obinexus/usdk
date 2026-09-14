import { UsdkError, Status } from '@usdk/contracts';

/**
 * The capability-vision contract. A conforming driver instance must
 * implement:
 *   describe(imageData: ImageData | Blob): Promise<{text: string, uncertainty: number}>
 *   isAvailable(): boolean
 *   label(): string - honest description
 */

export function checkVisionCapability(instance) {
  const missing = [];
  for (const m of ['describe', 'isAvailable', 'label']) {
    if (typeof instance?.[m] !== 'function') missing.push(`${m}()`);
  }
  return missing;
}

export function assertVisionCapability(instance) {
  const missing = checkVisionCapability(instance);
  if (missing.length > 0) {
    throw new UsdkError(Status.INVALID_ARGUMENT, `object does not implement capability-vision: missing ${missing.join(', ')}`);
  }
  return instance;
}
