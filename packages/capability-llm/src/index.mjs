import { UsdkError, Status } from '@usdk/contracts';

/**
 * The capability-llm contract - motivated by the PDF's Hypothesis III
 * (page 4, "Base LLM Module"; see docs/RESEARCH_REVIEW.md section 5.1).
 * This is an ADAPTER capability: implementing it does not create or
 * train new model weights, per this task's own explicit instruction.
 *
 * A conforming driver instance (the object create() returns) must
 * implement:
 *   generate(prompt: string, evidenceRefs: EvidenceRef[]): Promise<string>
 *   isAvailable(): boolean
 *   label(): string  - a short, honest identifier shown in the UI, e.g.
 *                       "[fixture]" or "[local: <model name>]" - see
 *                       docs/UAGENT_ARCHITECTURE.md "Do not present
 *                       fixture responses as real model inference."
 */

/** @param {any} instance @returns {string[]} list of missing members, empty if conformant */
export function checkLlmCapability(instance) {
  const missing = [];
  if (!instance || typeof instance.generate !== 'function') missing.push('generate(prompt, evidenceRefs)');
  if (typeof instance?.isAvailable !== 'function') missing.push('isAvailable()');
  if (typeof instance?.label !== 'function') missing.push('label()');
  return missing;
}

/** Throws if `instance` does not implement the capability-llm contract. */
export function assertLlmCapability(instance) {
  const missing = checkLlmCapability(instance);
  if (missing.length > 0) {
    throw new UsdkError(Status.INVALID_ARGUMENT, `object does not implement capability-llm: missing ${missing.join(', ')}`);
  }
  return instance;
}
