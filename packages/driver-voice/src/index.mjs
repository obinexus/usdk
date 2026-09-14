/**
 * Web Speech API-backed capability-voice driver. Browser-only.
 *
 * Honesty notes (see docs/UAGENT_ARCHITECTURE.md "Voice, accessibility,
 * and conversation" for the full pipeline-stage table):
 * - SpeechRecognition, in every Chrome-family browser that implements
 *   it, sends captured audio to a cloud speech-to-text service to
 *   produce a transcript - this is NOT local/offline processing, and
 *   this driver's label() says so explicitly. Firefox does not ship
 *   SpeechRecognition at all as of this writing (unverified in THIS
 *   environment - no browser engine comparison was run here; stated as
 *   general public knowledge about the API, not measured).
 * - SpeechSynthesis (`speechSynthesis.speak`) is typically local
 *   (OS/browser TTS voices), but this is also not independently verified
 *   in this environment - `isAvailable()` only confirms the API exists,
 *   not which backend a given browser/OS uses for it.
 */

export const descriptor = { name: 'driver-voice', version: '0.1', kind: 'voice' };

function hasSpeechRecognition() {
  return typeof globalThis.SpeechRecognition === 'function' || typeof globalThis.webkitSpeechRecognition === 'function';
}
function hasSpeechSynthesis() {
  return typeof globalThis.speechSynthesis === 'object' && typeof globalThis.SpeechSynthesisUtterance === 'function';
}

// A real failure mode, reproduced directly while testing this driver in
// an actual browser pane: some browser/OS/headless combinations never
// fire `onend` or `onerror` after `speechSynthesis.speak()` at all (no
// audio output device, no TTS voice actually installed despite
// `getVoices()` returning entries, etc.) - `speaking` silently goes back
// to `false` but neither callback runs. Without a fallback, the awaited
// Promise in @usdk/host-browser's dispatch function
// (`await new Promise((resolve) => voiceDriver.speak(...))`) hangs
// forever, which blocks usdk-core's post-commit dispatch from ever
// returning - the round already committed, but the UI never learns
// that, and stays on "Thinking..." permanently. See
// docs/UAGENT_ARCHITECTURE.md "Voice, accessibility, and conversation".
const DEFAULT_SPEAK_FALLBACK_TIMEOUT_MS = 15000;

export function create(options = {}) {
  const speakFallbackTimeoutMs = options.speakFallbackTimeoutMs ?? DEFAULT_SPEAK_FALLBACK_TIMEOUT_MS;
  const RecognitionCtor = globalThis.SpeechRecognition ?? globalThis.webkitSpeechRecognition;
  /** @type {SpeechSynthesisUtterance | null} */
  let currentUtterance = null;

  return Object.freeze({
    /**
     * @param {(text: string, isFinal: boolean) => void} onTranscript
     * @param {(reason: string) => void} onError
     * @returns {() => void} stop function
     */
    startListening(onTranscript, onError) {
      if (!hasSpeechRecognition()) {
        onError('SpeechRecognition is not available in this browser');
        return () => {};
      }
      const recognition = new RecognitionCtor();
      recognition.continuous = true;
      recognition.interimResults = true;
      recognition.onresult = (event) => {
        for (let i = event.resultIndex; i < event.results.length; i++) {
          const result = event.results[i];
          onTranscript(result[0].transcript, result.isFinal);
        }
      };
      recognition.onerror = (event) => onError(String(event.error ?? 'unknown speech recognition error'));
      recognition.start();
      return () => {
        try { recognition.stop(); } catch { /* already stopped */ }
      };
    },

    /**
     * @param {string} text @param {string} turnId @param {() => void} onDone
     * @returns {() => void} cancel function
     */
    speak(text, turnId, onDone) {
      if (!hasSpeechSynthesis()) {
        onDone();
        return () => {};
      }
      const utterance = new SpeechSynthesisUtterance(text);
      let finished = false;
      let fallbackTimer;
      const finish = () => {
        if (finished) return;
        finished = true;
        clearTimeout(fallbackTimer);
        currentUtterance = null;
        onDone();
      };
      utterance.onend = finish;
      utterance.onerror = finish;
      // See DEFAULT_SPEAK_FALLBACK_TIMEOUT_MS above - guards against
      // onend/onerror never firing at all.
      fallbackTimer = setTimeout(finish, speakFallbackTimeoutMs);
      currentUtterance = utterance;
      globalThis.speechSynthesis.speak(utterance);
      return () => {
        if (currentUtterance === utterance) {
          globalThis.speechSynthesis.cancel();
          finish();
        }
      };
    },

    stopSpeaking() {
      if (hasSpeechSynthesis()) globalThis.speechSynthesis.cancel();
      currentUtterance = null;
    },

    isAvailable() {
      return hasSpeechRecognition() || hasSpeechSynthesis();
    },

    label() {
      const sttNote = hasSpeechRecognition()
        ? 'speech-to-text: browser Web Speech API, typically NOT offline (audio may be sent to a cloud service)'
        : 'speech-to-text: unavailable in this browser';
      const ttsNote = hasSpeechSynthesis() ? 'text-to-speech: browser Web Speech API' : 'text-to-speech: unavailable in this browser';
      return `[${sttNote}; ${ttsNote}]`;
    },
  });
}
