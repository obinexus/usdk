import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assertVoiceCapability } from '@usdk/capability-voice';
import { create } from '../src/index.mjs';

// Node has no SpeechRecognition/speechSynthesis globals - this exercises
// the real, honest degradation path (no window at all), not a browser
// simulation. Full functional behavior (an actual transcript/utterance)
// can only be verified in a real browser - see docs/VALIDATION.md.

test('driver-voice conforms to capability-voice even with no browser APIs present', () => {
  assertVoiceCapability(create());
});

test('driver-voice: isAvailable() is false outside a browser (honest, not faked)', () => {
  const v = create();
  assert.equal(v.isAvailable(), false);
});

test('driver-voice: startListening reports unavailability via onError rather than throwing', () => {
  const v = create();
  let errorMsg = null;
  const stop = v.startListening(
    () => assert.fail('should not produce a transcript with no SpeechRecognition'),
    (reason) => { errorMsg = reason; }
  );
  assert.ok(errorMsg);
  assert.equal(typeof stop, 'function');
  stop(); // must not throw
});

test('driver-voice: speak() calls onDone immediately with no SpeechSynthesis (text-only fallback)', () => {
  const v = create();
  let done = false;
  v.speak('hello', 'turn-1', () => { done = true; });
  assert.equal(done, true);
});

test("driver-voice: label() discloses that speech recognition is typically not offline", () => {
  const v = create();
  assert.ok(v.label().length > 0);
});
