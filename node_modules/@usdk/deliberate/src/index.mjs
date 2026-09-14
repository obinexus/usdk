import { sha256Hex, createVote, Role, Verdict, UsdkError, Status } from '@usdk/contracts';

export const descriptor = { name: 'deliberate', version: '0.1', kind: 'role' };

/**
 * @param {Object} config
 * @param {{generate: (prompt: string, evidenceRefs: import('@usdk/contracts').EvidenceRef[]) => Promise<string>}} config.llmDriver
 *   an already-constructed capability-llm driver instance (e.g. from
 *   @usdk/driver-llm-fixture's create()) - injected by the caller
 *   (typically @usdk/host-browser or @usdk/host-local via @usdk/loader),
 *   never imported by this package directly.
 * @param {string} [config.partyId]
 */
export function create(config) {
  if (!config || typeof config.llmDriver?.generate !== 'function') {
    throw new UsdkError(Status.INVALID_ARGUMENT, "create() requires config.llmDriver with a generate(prompt, evidenceRefs) method");
  }
  const { llmDriver } = config;
  const partyId = config.partyId ?? 'usdk-deliberate';

  const HISTORY_CAP = 64;
  /** @type {string[]} ring buffer of digests this instance has generated */
  const history = [];

  return Object.freeze({
    /**
     * Calls the injected driver to generate a response, and records its
     * digest so a later vote() on a candidate built from it can
     * recognize it as self-generated.
     * @param {string} prompt
     * @param {import('@usdk/contracts').EvidenceRef[]} evidenceRefs
     * @returns {Promise<string>}
     */
    async propose(prompt, evidenceRefs = []) {
      const response = await llmDriver.generate(prompt, evidenceRefs);
      const digest = await sha256Hex(response);
      history.push(digest);
      if (history.length > HISTORY_CAP) history.shift();
      return response;
    },

    /**
     * Records `responseText` into this instance's self-generated
     * history WITHOUT calling the LLM driver - used for non-text
     * candidate kinds this role does not itself author, such as a
     * structured robotics action assembled by the host UI (see
     * @usdk/host-browser and docs/UAGENT_ARCHITECTURE.md "Robotics").
     *
     * DOCUMENTED TRADEOFF, stated plainly rather than hidden: for a
     * candidate registered this way, deliberate's "self-consistency"
     * check degrades to "the host explicitly told me to vouch for this
     * exact content" - it is no longer evidence that deliberate itself
     * produced the content via inference. This is a deliberate,
     * disclosed scope choice for the first version (there is no second
     * generation-capable role to author structured robotics proposals),
     * not a claim that deliberate is independently validating a
     * robotics action's content - that is verify's and, at execution
     * time, the robotics driver's job (bounds re-validation,
     * docs/UAGENT_ARCHITECTURE.md "Robotics").
     * @param {string} responseText
     */
    async proposeDirect(responseText) {
      const digest = await sha256Hex(responseText);
      history.push(digest);
      if (history.length > HISTORY_CAP) history.shift();
      return responseText;
    },

    /**
     * Distinct check: REJECT "not-self-generated" if
     * candidate.proposedResponse does not match (by digest) anything
     * this instance produced via propose() - a rubber-stamp vote on an
     * arbitrary candidate is exactly what this check prevents.
     * @param {import('@usdk/contracts').Candidate} candidate
     * @returns {Promise<import('@usdk/contracts').Vote>}
     */
    async vote(candidate) {
      const base = { partyId, role: Role.DELIBERATE, candidateDigestSeen: candidate.contentDigest };
      const digest = await sha256Hex(candidate.proposedResponse);
      if (!history.includes(digest)) {
        return createVote({
          ...base,
          verdict: Verdict.REJECT,
          reasonCode: 'not-self-generated',
          reasonDetail: 'proposedResponse does not match anything this instance produced via propose()',
        });
      }
      return createVote({
        ...base,
        verdict: Verdict.ACCEPT,
        reasonCode: 'self-consistent',
        reasonDetail: 'proposedResponse matches a response this instance generated',
      });
    },

    destroy() {
      history.length = 0;
    },
  });
}
