import { UsdkError, Status } from './status.mjs';

/**
 * The five capabilities from the PDF's Hypothesis III (page 4, Figure 4
 * - Base LLM Module, Voice Interface, Vision Module, Accessibility
 * Features, Robotics Interface) - see docs/RESEARCH_REVIEW.md section
 * 5.1. Deliberately a SEPARATE enum from Role (vote.mjs): capabilities
 * are replaceable implementations a driver provides; roles are the
 * three parties that vote. A capability is never a voting party, and a
 * role never doubles as a capability - see docs/UAGENT_ARCHITECTURE.md
 * "Five capabilities are not five voting parties."
 */
export const CapabilityKind = Object.freeze({
  LLM: 'llm',
  VOICE: 'voice',
  VISION: 'vision',
  A11Y: 'a11y',
  ROBOTICS: 'robotics',
});

/**
 * @typedef {Object} ManifestDependency
 * @property {string} capability - the depended-on manifest name
 * @property {string} minVersion - "major.minor", compared numerically
 */

/**
 * @typedef {Object} Manifest
 * @property {string} name - unique module name, e.g. "driver-llm-fixture"
 * @property {string} version - "major.minor"
 * @property {string} kind - one of CapabilityKind, or "role" for
 *   perceive/deliberate/verify, or "core"/"loader" for infrastructure
 * @property {string} entry - browser-local: an import()-able module
 *   specifier, relative to the manifest's own package root
 * @property {ManifestDependency[]} dependencies
 */

/**
 * Parses and validates a manifest object (already-parsed JSON - this
 * function does not read files, callers do, since a browser and a
 * Node.js host read manifests differently). Throws UsdkError on a
 * missing/malformed required field, exactly as
 * src/ffi/manifest.c/usdk_manifest_parse does for the native ABI - see
 * docs/ABI.md "Module manifests and dependency resolution" for why this
 * validation existing at all matters (a concrete cross-file struct
 * mismatch bug in the supplied research, docs/RESEARCH_REVIEW.md
 * section 1.3.10, is the reason).
 * @param {unknown} raw
 * @returns {Manifest}
 */
export function parseManifest(raw) {
  const fail = (msg) => {
    throw new UsdkError(Status.INVALID_ARGUMENT, msg);
  };
  if (!raw || typeof raw !== 'object') fail('manifest must be an object');
  const m = /** @type {any} */ (raw);
  if (typeof m.name !== 'string' || !m.name) fail("manifest missing 'name'");
  if (typeof m.version !== 'string' || !/^\d+\.\d+$/.test(m.version)) {
    fail("manifest missing or invalid 'version' (expected \"major.minor\")");
  }
  if (typeof m.kind !== 'string' || !m.kind) fail("manifest missing 'kind'");
  if (typeof m.entry !== 'string' || !m.entry) fail("manifest missing 'entry'");
  const dependencies = Array.isArray(m.dependencies) ? m.dependencies : [];
  for (const d of dependencies) {
    if (typeof d.capability !== 'string' || typeof d.minVersion !== 'string') {
      fail('manifest dependency entries need "capability" and "minVersion"');
    }
  }
  return Object.freeze({
    name: m.name,
    version: m.version,
    kind: m.kind,
    entry: m.entry,
    dependencies: dependencies.map((d) => Object.freeze({ ...d })),
  });
}

/** Compares two "major.minor" version strings: -1, 0, or 1. */
export function compareVersions(a, b) {
  const [aMaj, aMin] = a.split('.').map(Number);
  const [bMaj, bMin] = b.split('.').map(Number);
  if (aMaj !== bMaj) return aMaj < bMaj ? -1 : 1;
  if (aMin !== bMin) return aMin < bMin ? -1 : 1;
  return 0;
}
