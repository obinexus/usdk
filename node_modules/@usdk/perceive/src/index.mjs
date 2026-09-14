import { sha256Hex, createVote, Role, Verdict } from '@usdk/contracts';

/** Loadable-module descriptor (see @usdk/loader). Not the consensus
 * "role" enum value directly - kind is checked against the manifest by
 * the loader; role (perceive) is what this instance votes as. */
export const descriptor = { name: 'perceive', version: '0.1', kind: 'role' };

/**
 * @param {Object} [config]
 * @param {boolean} [config.requireEvidence] default true - reject a
 *   candidate that cites zero evidence, rather than abstaining, since
 *   "no evidence at all" is determinable outright, not genuine
 *   uncertainty this role cannot resolve.
 * @param {string} [config.partyId]
 */
export function create(config = {}) {
  const requireEvidence = config.requireEvidence ?? true;
  const partyId = config.partyId ?? 'usdk-perceive';
  /** @type {Map<string, {digest: string, uncertainty: number}>} */
  const observed = new Map();
  let nextSeq = 0;

  return Object.freeze({
    /**
     * The role's primary operation: records `rawInput` under a freshly
     * generated evidence id, and returns the evidence_ref a candidate
     * should cite to reference this observation.
     * @param {string} rawInput
     * @param {number} uncertainty
     * @returns {Promise<import('@usdk/contracts').EvidenceRef>}
     */
    async observe(rawInput, uncertainty) {
      const clamped = Math.min(1, Math.max(0, uncertainty));
      const digest = await sha256Hex(rawInput);
      const evidenceId = `ev-${nextSeq++}`;
      observed.set(evidenceId, { digest, uncertainty: clamped });
      return Object.freeze({ evidenceId, evidenceDigest: digest, uncertainty: clamped });
    },

    /**
     * Distinct check: every evidence_ref must match an observation this
     * instance actually recorded (same evidenceId and digest). Never
     * returns ABSTAIN - a missing/fabricated reference is determinable,
     * not irreducibly uncertain.
     * @param {import('@usdk/contracts').Candidate} candidate
     * @returns {Promise<import('@usdk/contracts').Vote>}
     */
    async vote(candidate) {
      const base = { partyId, role: Role.PERCEIVE, candidateDigestSeen: candidate.contentDigest };

      if (requireEvidence && candidate.evidenceRefs.length === 0) {
        return createVote({
          ...base,
          verdict: Verdict.REJECT,
          reasonCode: 'no-evidence-cited',
          reasonDetail: 'candidate cites zero evidence references',
        });
      }
      for (const ref of candidate.evidenceRefs) {
        const rec = observed.get(ref.evidenceId);
        if (!rec || rec.digest !== ref.evidenceDigest) {
          return createVote({
            ...base,
            verdict: Verdict.REJECT,
            reasonCode: 'evidence-not-found',
            reasonDetail: `evidence_ref '${ref.evidenceId}' does not match any observation this instance recorded`,
          });
        }
      }
      return createVote({
        ...base,
        verdict: Verdict.ACCEPT,
        reasonCode: 'evidence-verified',
        reasonDetail: 'every cited evidence_ref matches an observation this instance recorded',
      });
    },

    destroy() {
      observed.clear();
    },
  });
}
