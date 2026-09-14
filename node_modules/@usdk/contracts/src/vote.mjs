/** Mirrors usdk_verdict_t (include/usdk/vote.h). */
export const Verdict = Object.freeze({
  ACCEPT: 'accept',
  REJECT: 'reject',
  ABSTAIN: 'abstain',
});

/** Mirrors usdk_role_t - the three consensus roles only. The five
 * capabilities (llm/voice/vision/a11y/robotics) are a SEPARATE enum
 * (see capability.mjs's CapabilityKind) - "Five capabilities are not
 * five voting parties" (this task's own instruction, restated in
 * docs/UAGENT_ARCHITECTURE.md). */
export const Role = Object.freeze({
  PERCEIVE: 'perceive',
  DELIBERATE: 'deliberate',
  VERIFY: 'verify',
});

/**
 * @typedef {Object} Vote
 * @property {string} partyId
 * @property {string} role - one of Role
 * @property {string} verdict - one of Verdict
 * @property {string} reasonCode - short, machine-readable
 * @property {string} reasonDetail - human-readable
 * @property {string} candidateDigestSeen - the digest of the candidate this vote was actually cast on
 * @property {number} votedAtMs
 */

/**
 * @param {Object} fields
 * @param {string} fields.partyId
 * @param {string} fields.role
 * @param {string} fields.verdict
 * @param {string} fields.reasonCode
 * @param {string} fields.reasonDetail
 * @param {string} fields.candidateDigestSeen
 * @returns {Vote}
 */
export function createVote({ partyId, role, verdict, reasonCode, reasonDetail, candidateDigestSeen }) {
  return Object.freeze({
    partyId,
    role,
    verdict,
    reasonCode,
    reasonDetail,
    candidateDigestSeen,
    votedAtMs: Date.now(),
  });
}
