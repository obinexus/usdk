import { createVote, Role, Verdict } from '@usdk/contracts';

export const descriptor = { name: 'verify', version: '0.1', kind: 'role' };

/**
 * @param {Object} [config]
 * @param {string[]} [config.allowedConstraints]
 * @param {number} [config.maxUncertainty] default 0.5 - caller-supplied
 *   policy, never a value this package treats as derived or proven (see
 *   docs/RESEARCH_REVIEW.md section 1.3.5/5.4).
 * @param {string} [config.partyId]
 */
export function create(config = {}) {
  const allowedConstraints = new Set(config.allowedConstraints ?? []);
  const maxUncertainty = config.maxUncertainty ?? 0.5;
  const partyId = config.partyId ?? 'usdk-verify';

  return Object.freeze({
    /**
     * Distinct check: (1) every constraint permitted, (2) every
     * evidence_ref's uncertainty within policy, (3) at least one
     * evidence_ref present. This does not check whether the evidence is
     * genuine (that is @usdk/perceive's check) or whether deliberate
     * actually generated the response (that is @usdk/deliberate's
     * check) - see docs/UAGENT_ARCHITECTURE.md "Give each party
     * distinct checks."
     * @param {import('@usdk/contracts').Candidate} candidate
     * @returns {Promise<import('@usdk/contracts').Vote>}
     */
    async vote(candidate) {
      const base = { partyId, role: Role.VERIFY, candidateDigestSeen: candidate.contentDigest };

      for (const c of candidate.constraints) {
        if (!allowedConstraints.has(c.constraintId)) {
          return createVote({
            ...base,
            verdict: Verdict.REJECT,
            reasonCode: 'constraint-not-permitted',
            reasonDetail: `constraint '${c.constraintId}' is not in this instance's allowedConstraints policy`,
          });
        }
      }

      if (candidate.evidenceRefs.length === 0) {
        return createVote({
          ...base,
          verdict: Verdict.REJECT,
          reasonCode: 'insufficient-evidence',
          reasonDetail: 'candidate cites zero evidence references',
        });
      }

      for (const ref of candidate.evidenceRefs) {
        if (ref.uncertainty > maxUncertainty) {
          return createVote({
            ...base,
            verdict: Verdict.REJECT,
            reasonCode: 'uncertainty-exceeds-policy',
            reasonDetail: `evidence_ref '${ref.evidenceId}' uncertainty ${ref.uncertainty.toFixed(4)} exceeds policy max ${maxUncertainty}`,
          });
        }
      }

      return createVote({
        ...base,
        verdict: Verdict.ACCEPT,
        reasonCode: 'constraints-and-evidence-sufficient',
        reasonDetail: 'every constraint is permitted and every evidence_ref is within the uncertainty policy',
      });
    },

    destroy() {},
  });
}
