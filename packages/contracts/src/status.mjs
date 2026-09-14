/**
 * Status codes, mirroring usdk_status_t (include/usdk/status.h) in the
 * native C SDK - not a strict 1:1 copy (JS has fewer categories worth
 * distinguishing, e.g. no separate struct-size-mismatch concept), but
 * intentionally overlapping names so a developer moving between the C
 * ABI and this JS layer recognizes the vocabulary.
 */
export const Status = Object.freeze({
  OK: 'ok',
  INVALID_ARGUMENT: 'invalid-argument',
  ABI_VERSION_MISMATCH: 'abi-version-mismatch',
  CAPABILITY_NOT_FOUND: 'capability-not-found',
  MODULE_LOAD_FAILED: 'module-load-failed',
  DEPENDENCY_CYCLE: 'dependency-cycle',
  DEPENDENCY_UNRESOLVED: 'dependency-unresolved',
  DEPENDENCY_VERSION_INCOMPATIBLE: 'dependency-version-incompatible',
  STALE_ROUND: 'stale-round',
  DUPLICATE_VOTE: 'duplicate-vote',
  PARTY_CONFLICT: 'party-conflict',
  CANDIDATE_MUTATED: 'candidate-mutated',
  DEADLINE_EXCEEDED: 'deadline-exceeded',
  ROUND_NOT_OPEN: 'round-not-open',
  NOT_IMPLEMENTED: 'not-implemented',
});

/** Thrown by @usdk/* functions instead of returning a bare error string,
 * so callers can `catch (e) { if (e instanceof UsdkError) ... }` and
 * still read a stable `e.status` code from the Status enum above. */
export class UsdkError extends Error {
  /** @param {string} status one of the Status values @param {string} message */
  constructor(status, message) {
    super(message);
    this.name = 'UsdkError';
    this.status = status;
  }
}
