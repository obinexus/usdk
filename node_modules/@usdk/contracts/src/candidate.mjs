import { sha256Hex, canonicalCandidateString } from './wire.mjs';
import { UsdkError, Status } from './status.mjs';

export const PROTOCOL_VERSION_V1 = 1;

/**
 * @typedef {Object} EvidenceRef
 * @property {string} evidenceId
 * @property {string} evidenceDigest - hex SHA-256 of the evidence content perceive recorded
 * @property {number} uncertainty - in [0,1]
 */

/**
 * @typedef {Object} Constraint
 * @property {string} constraintId - e.g. "workspace:temp-only"
 */

/**
 * @typedef {Object} Candidate
 * @property {number} protocolVersion
 * @property {number} policyVersion
 * @property {string} sessionId
 * @property {number} roundId
 * @property {string} candidateId
 * @property {string} contentDigest - hex SHA-256, set by createCandidate, never by the caller
 * @property {EvidenceRef[]} evidenceRefs
 * @property {string} proposedResponse
 * @property {Constraint[]} constraints
 * @property {number} deadlineMs - Date.now()-comparable absolute deadline
 */

/**
 * Immutable once created - see docs/CONSENSUS_PROTOCOL.md "The
 * candidate" (native) and docs/UAGENT_ARCHITECTURE.md for the JS-layer
 * correspondence. `Object.freeze` (including the nested evidence/
 * constraint arrays) is the JS equivalent of the C struct having no
 * setters: a caller CAN still reach in with `Object.defineProperty` or
 * similar, exactly as a C caller could still overwrite a struct field
 * through raw memory access - candidate mutation detection
 * (@usdk/core) exists precisely because neither guarantee is airtight,
 * only because both make accidental mutation loud instead of silent.
 *
 * @param {Object} fields
 * @param {number} fields.policyVersion
 * @param {string} fields.sessionId
 * @param {number} fields.roundId
 * @param {string} fields.candidateId
 * @param {EvidenceRef[]} fields.evidenceRefs
 * @param {string} fields.proposedResponse
 * @param {Constraint[]} fields.constraints
 * @param {number} fields.deadlineMs
 * @returns {Promise<Candidate>}
 */
export async function createCandidate({
  policyVersion,
  sessionId,
  roundId,
  candidateId,
  evidenceRefs = [],
  proposedResponse,
  constraints = [],
  deadlineMs,
}) {
  if (!sessionId || typeof sessionId !== 'string') {
    throw new UsdkError(Status.INVALID_ARGUMENT, 'sessionId must be a non-empty string');
  }
  if (!candidateId || typeof candidateId !== 'string') {
    throw new UsdkError(Status.INVALID_ARGUMENT, 'candidateId must be a non-empty string');
  }
  if (typeof proposedResponse !== 'string') {
    throw new UsdkError(Status.INVALID_ARGUMENT, 'proposedResponse must be a string');
  }
  if (!Number.isFinite(deadlineMs)) {
    throw new UsdkError(Status.INVALID_ARGUMENT, 'deadlineMs must be a finite number');
  }

  const draft = {
    protocolVersion: PROTOCOL_VERSION_V1,
    policyVersion,
    sessionId,
    roundId,
    candidateId,
    evidenceRefs: evidenceRefs.map((e) => Object.freeze({ ...e })),
    proposedResponse,
    constraints: constraints.map((k) => Object.freeze({ ...k })),
    deadlineMs,
  };
  const contentDigest = await sha256Hex(canonicalCandidateString(draft));
  return Object.freeze({ ...draft, contentDigest });
}

/**
 * Recomputes the canonical digest over `c` and compares it to
 * `c.contentDigest` - see docs/CONSENSUS_PROTOCOL.md "Candidate mutation
 * detection". @param {Candidate} c @returns {Promise<boolean>}
 */
export async function candidateDigestMatches(c) {
  const recomputed = await sha256Hex(canonicalCandidateString(c));
  return recomputed === c.contentDigest;
}
