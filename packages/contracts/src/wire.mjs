/**
 * Canonical digest encoding for a candidate - the JS-side analogue of
 * include/usdk/wire.h in the native C SDK, with a deliberately different
 * concrete encoding (stable-key-order JSON instead of a fixed-width
 * binary layout): this package has no struct-padding or pointer-width
 * concerns to guard against (there is no in-process ABI boundary here,
 * only JS objects), so the byte-exactness discipline the C encoder
 * needs is not required for correctness - what IS required, and what
 * this file provides, is that the same logical candidate content always
 * produces the same digest, and that no two different contents collide
 * through an ambiguous encoding (achieved by a fixed field order and
 * explicit JSON.stringify of every field, never relying on object key
 * enumeration order for anything other than this file's own fixed list).
 *
 * Uses the Web Crypto SubtleCrypto API (`crypto.subtle`), which is
 * available identically in every modern browser and in Node.js (18.5+
 * globally, without an import) - so this file runs unmodified in both
 * environments. Verified directly against a known-answer vector
 * (sha256("abc")) before use - see packages/contracts/test/wire.test.mjs.
 */

const textEncoder = new TextEncoder();

/** @param {string} text @returns {Promise<string>} lowercase hex SHA-256 digest */
export async function sha256Hex(text) {
  const digest = await crypto.subtle.digest('SHA-256', textEncoder.encode(text));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

/**
 * Encodes every candidate field except `contentDigest` itself into one
 * canonical string - fixed field order, JSON.stringify per field (never
 * a single JSON.stringify(candidate), which would depend on key
 * insertion order and would also try to serialize contentDigest before
 * it exists).
 * @param {import('./candidate.mjs').Candidate} c
 */
export function canonicalCandidateString(c) {
  return [
    'usdk-candidate-v1',
    JSON.stringify(c.protocolVersion),
    JSON.stringify(c.policyVersion),
    JSON.stringify(c.sessionId),
    JSON.stringify(c.roundId),
    JSON.stringify(c.candidateId),
    JSON.stringify(c.evidenceRefs.map((e) => [e.evidenceId, e.evidenceDigest, e.uncertainty])),
    JSON.stringify(c.proposedResponse),
    JSON.stringify(c.constraints.map((k) => k.constraintId)),
    JSON.stringify(c.deadlineMs),
  ].join('');
}
