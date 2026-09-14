import { UsdkError, Status } from '@usdk/contracts';

export const descriptor = { name: 'driver-llm-local', version: '0.1', kind: 'llm' };

/**
 * No real local-inference runtime/model was confirmed available in this
 * environment (checked: no ONNX Runtime Web/WebGPU model runtime bundled
 * or reachable offline, no local model weights supplied). This driver
 * exists so the manifest/loader path has something to resolve to and so
 * a host can show an honest "not available" message through the exact
 * same code path as a genuinely-missing capability - it never generates
 * a response, fixture-labeled or otherwise. See
 * docs/IMPLEMENTATION_STATUS.md for exactly what would need to be
 * supplied (a browser-compatible model runtime + compatible weights, or
 * a native driver reached through @usdk/host-local's connected mode) to
 * replace this with a real driver.
 */
export function create() {
  return Object.freeze({
    async generate() {
      throw new UsdkError(
        Status.NOT_IMPLEMENTED,
        'no local LLM runtime is available in this environment - use @usdk/driver-llm-fixture for a labeled, deterministic fixture instead'
      );
    },
    isAvailable() {
      return false;
    },
    label() {
      return '[local LLM unavailable - no runtime/model supplied in this environment]';
    },
  });
}
