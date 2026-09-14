/**
 * A deterministic, offline capability-llm driver. Loadable both via
 * @usdk/loader's dynamic `import()` mechanism (export `descriptor` +
 * `create`, per the module contract in @usdk/loader/src/browser-loader.mjs)
 * and via direct static import (as @usdk/deliberate's own tests and the
 * standalone examples do) - the same dual-path pattern the native
 * usdk-perceive/deliberate/verify libraries use (docs/ABI.md "Entry
 * point").
 */

export const descriptor = { name: 'driver-llm-fixture', version: '0.1', kind: 'llm' };

/** @param {Object} [config] @param {number} [config.latencyMs] simulated processing delay, default 0 */
export function create(config = {}) {
  const latencyMs = config.latencyMs ?? 0;

  return Object.freeze({
    /**
     * Deterministic: the same prompt + evidence always produces the
     * same text - this is what makes tests and demos built on this
     * driver reproducible, unlike a real inference backend.
     * @param {string} prompt
     * @param {import('@usdk/contracts').EvidenceRef[]} evidenceRefs
     * @returns {Promise<string>}
     */
    async generate(prompt, evidenceRefs = []) {
      if (latencyMs > 0) await new Promise((r) => setTimeout(r, latencyMs));
      const maxUncertainty = evidenceRefs.reduce((m, e) => Math.max(m, e.uncertainty), 0);
      return `[fixture] deterministic fixture response - not real model inference. ` +
        `evidence_count=${evidenceRefs.length} max_uncertainty=${maxUncertainty.toFixed(4)} prompt="${prompt}"`;
    },
    isAvailable() {
      return true;
    },
    label() {
      return '[fixture: deterministic, offline, not real model inference]';
    },
  });
}
