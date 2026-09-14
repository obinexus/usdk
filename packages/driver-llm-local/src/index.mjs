import { UsdkError, Status } from '@usdk/contracts';

export const descriptor = { name: 'driver-llm-local', version: '0.1', kind: 'llm' };

/**
 * Real local-inference LLM driver, backed by WebLLM
 * (https://github.com/mlc-ai/web-llm) - runs entirely in the browser tab
 * via WebAssembly + WebGPU, no server, no account, no API key. This is
 * still an ADAPTER capability, not new model weights or training - the
 * model itself is one of WebLLM's own prebuilt, already-trained
 * releases (see MODEL_ID below), downloaded and cached by the browser,
 * not produced by this project.
 *
 * The WebLLM LIBRARY is loaded via a runtime `import()` of a CDN ESM
 * URL, not an npm dependency - this keeps the workspace's own
 * `npm install --offline` zero-network-access property intact (see
 * docs/UAGENT_ARCHITECTURE.md "Why zero dependencies"): nothing here
 * changes what `npm install` fetches. The CDN fetch, and the much
 * larger model-weight download it triggers, happen ONLY when
 * `loadModel()` is explicitly called - never on module import, never on
 * `create()`, so simply loading this page does not start a multi-
 * hundred-megabyte download. `isAvailable()` reports whether WebGPU
 * exists at all; `generate()` throws NOT_IMPLEMENTED (the exact
 * honest-degradation pattern every other driver in this workspace uses)
 * until `loadModel()` has actually been awaited to completion.
 *
 * Module-level (not per-instance) engine state is intentional: an ES
 * module is a singleton per URL in a given browsing context, so a UI
 * that calls the exported `loadModel()` directly to pre-warm the model
 * BEFORE constructing a @usdk/host-browser session, and @usdk/loader's
 * own later `dynamic import()` of this same module (which happens
 * inside ConversationSession.init()), both see the SAME already-loaded
 * engine - no redundant download or re-initialization either way.
 */

export const MODEL_ID = 'SmolLM2-360M-Instruct-q4f16_1-MLC';
const WEBLLM_CDN_URL = 'https://esm.run/@mlc-ai/web-llm';

/** @type {import('@mlc-ai/web-llm').MLCEngine | null} */
let engine = null;
/** @type {Promise<void> | null} */
let loadPromise = null;
let loadError = null;

function hasWebGpu() {
  return typeof navigator !== 'undefined' && 'gpu' in navigator;
}

/**
 * Downloads (first time only - cached by the browser after that) and
 * initializes the model. Idempotent: a second call while one is already
 * in flight (or already complete) returns the same promise / resolves
 * immediately rather than starting a second download.
 * @param {(report: {text: string, progress?: number}) => void} [onProgress]
 * @returns {Promise<void>}
 */
export async function loadModel(onProgress) {
  if (!hasWebGpu()) {
    throw new UsdkError(Status.NOT_IMPLEMENTED, 'WebGPU is not available in this browser - real local inference requires it (navigator.gpu is undefined)');
  }
  if (engine) return; // already loaded
  if (loadPromise) return loadPromise; // already loading - join the same attempt

  loadPromise = (async () => {
    let webllm;
    try {
      webllm = await import(/* webpackIgnore: true */ WEBLLM_CDN_URL);
    } catch (e) {
      loadError = e;
      loadPromise = null;
      throw new UsdkError(Status.MODULE_LOAD_FAILED, `failed to load @mlc-ai/web-llm from ${WEBLLM_CDN_URL}: ${e && e.message}`);
    }
    try {
      engine = await webllm.CreateMLCEngine(MODEL_ID, {
        initProgressCallback: (report) => onProgress?.(report),
      });
    } catch (e) {
      loadError = e;
      loadPromise = null;
      throw new UsdkError(Status.IO, `failed to initialize WebLLM model '${MODEL_ID}': ${e && e.message}`);
    }
  })();
  return loadPromise;
}

export function isModelLoaded() {
  return engine !== null;
}

export function create() {
  return Object.freeze({
    /**
     * @param {string} prompt
     * @param {import('@usdk/contracts').EvidenceRef[]} evidenceRefs
     * @returns {Promise<string>}
     */
    async generate(prompt, evidenceRefs = []) {
      if (!engine) {
        throw new UsdkError(
          Status.NOT_IMPLEMENTED,
          loadError
            ? `local model failed to load earlier: ${loadError.message}`
            : `no local model is loaded yet - call loadModel() first (see @usdk/driver-llm-local's module docstring); use @usdk/driver-llm-fixture for a labeled, deterministic fixture instead`
        );
      }
      const evidenceNote = evidenceRefs.length > 0
        ? `\n\n(${evidenceRefs.length} evidence reference(s) recorded, max uncertainty ${Math.max(...evidenceRefs.map((e) => e.uncertainty)).toFixed(2)})`
        : '';
      const reply = await engine.chat.completions.create({
        messages: [
          { role: 'system', content: 'You are a helpful, concise assistant.' },
          { role: 'user', content: prompt },
        ],
      });
      const text = reply.choices?.[0]?.message?.content ?? '';
      return `[local: ${MODEL_ID}] ${text}${evidenceNote}`;
    },

    isAvailable() {
      return hasWebGpu();
    },

    label() {
      if (!hasWebGpu()) return '[local LLM unavailable - no WebGPU in this browser]';
      if (!engine) return `[local: ${MODEL_ID} - not loaded yet, call loadModel()]`;
      return `[local: ${MODEL_ID} - loaded, real inference via WebLLM/WebGPU]`;
    },

    loadModel,
    isModelLoaded,
  });
}
