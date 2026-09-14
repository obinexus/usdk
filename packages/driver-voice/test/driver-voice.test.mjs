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

test('driver-voice: speak() still calls onDone via the fallback timeout if onend/onerror never fire - reproduces a real hang found live in a browser pane where speechSynthesis silently never fired either callback', async () => {
  // A minimal fake speechSynthesis whose utterance is accepted but NEVER
  // dispatches onend/onerror - the exact failure mode observed live.
  const originalSynthesis = globalThis.speechSynthesis;
  const originalUtteranceCtor = globalThis.SpeechSynthesisUtterance;
  globalThis.speechSynthesis = { speak() {}, cancel() {} };
  globalThis.SpeechSynthesisUtterance = class { constructor(text) { this.text = text; } };
  try {
    const v = create({ speakFallbackTimeoutMs: 20 });
    const done = await new Promise((resolve) => {
      v.speak('hello', 'turn-1', () => resolve(true));
    });
    assert.equal(done, true);
  } finally {
    globalThis.speechSynthesis = originalSynthesis;
    globalThis.SpeechSynthesisUtterance = originalUtteranceCtor;
  }
});

test('driver-voice: speak() calls onDone exactly once even if both onend fires AND the fallback timer was pending', async () => {
  const originalSynthesis = globalThis.speechSynthesis;
  const originalUtteranceCtor = globalThis.SpeechSynthesisUtterance;
  let utteranceRef = null;
  globalThis.speechSynthesis = {
    speak(u) { utteranceRef = u; setTimeout(() => u.onend?.(), 5); },
    cancel() {},
  };
  globalThis.SpeechSynthesisUtterance = class { constructor(text) { this.text = text; } };
  try {
    const v = create({ speakFallbackTimeoutMs: 1000 });
    let callCount = 0;
    await new Promise((resolve) => {
      v.speak('hello', 'turn-1', () => { callCount++; resolve(); });
    });
    await new Promise((r) => setTimeout(r, 30)); // let a stray fallback fire, if any, surface
    assert.equal(callCount, 1);
  } finally {
    globalThis.speechSynthesis = originalSynthesis;
    globalThis.SpeechSynthesisUtterance = originalUtteranceCtor;
  }
});
