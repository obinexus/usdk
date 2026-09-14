import { UsdkError, Status } from '@usdk/contracts';

/**
 * The capability-voice contract. A conforming driver instance must
 * implement:
 *   startListening(onTranscript: (text: string, isFinal: boolean) => void,
 *                   onError: (reason: string) => void): () => void  - returns a stop function
 *   speak(text: string, turnId: string, onDone: () => void): () => void  - returns a cancel function
 *   stopSpeaking(): void
 *   isAvailable(): boolean
 *   label(): string - honest description, e.g. "[browser Web Speech API - not offline]"
 *
 * `speak`/`startListening` both take/produce a `turnId` so a caller
 * (see @usdk/host-browser) can detect and discard a stale callback from
 * a turn that has since been superseded or cancelled - see
 * docs/UAGENT_ARCHITECTURE.md "Prevention of stale speech."
 */

export function checkVoiceCapability(instance) {
  const missing = [];
  for (const m of ['startListening', 'speak', 'stopSpeaking', 'isAvailable', 'label']) {
    if (typeof instance?.[m] !== 'function') missing.push(`${m}()`);
  }
  return missing;
}

export function assertVoiceCapability(instance) {
  const missing = checkVoiceCapability(instance);
  if (missing.length > 0) {
    throw new UsdkError(Status.INVALID_ARGUMENT, `object does not implement capability-voice: missing ${missing.join(', ')}`);
  }
  return instance;
}
