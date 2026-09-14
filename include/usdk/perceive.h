#ifndef USDK_PERCEIVE_H
#define USDK_PERCEIVE_H

#include "usdk/plugin.h"

USDK_BEGIN_DECLS

/* usdk-perceive: converts incoming information into structured
 * observations and evidence-bearing context, and represents uncertainty
 * and missing evidence - see docs/ARCHITECTURE.md.
 *
 * `cfg->data` is an opaque JSON payload with one optional field:
 * {"require_evidence": true}  (default true) - if true, usdk_perceive_vote
 * rejects a candidate that cites zero evidence_refs rather than
 * abstaining, since "no evidence at all" is a determinable defect this
 * role can check directly, not genuine uncertainty about something it
 * cannot know.
 *
 * These are ordinary exported functions, directly callable by anything
 * that links usdk_perceive - a test or example does not need to go
 * through usdk-ffi to exercise this role in isolation. This same
 * implementation additionally exports usdk_plugin_query_v1
 * (src/perceive/perceive.c) so it can also be discovered and loaded
 * dynamically like any other module. Not thread-safe per instance. */

USDK_EXPORT usdk_status_t USDK_CALL usdk_perceive_create(
    const usdk_config_t* cfg, usdk_role_instance_t** out);
USDK_EXPORT void USDK_CALL usdk_perceive_destroy(usdk_role_instance_t* inst);

/* The role's primary operation: records `raw_input` (borrowed only for
 * this call) under a freshly generated evidence_id, stores its digest
 * and the caller-declared `uncertainty` (clamped into [0,1] if out of
 * range - out-of-range input is a caller error this function corrects
 * rather than propagates), and returns the usdk_evidence_ref_t a
 * candidate should cite to reference this observation. This is what
 * makes a later usdk_perceive_vote able to tell a genuine reference from
 * a fabricated one - see docs/ARCHITECTURE.md's table. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_perceive_observe(
    usdk_role_instance_t* inst,
    usdk_buffer_t raw_input,
    double uncertainty,
    usdk_evidence_ref_t* out_ref);

/* Distinct check: every evidence_ref on `candidate` must match an
 * observation this instance actually recorded (same evidence_id and
 * digest) - REJECT with reason_code "evidence-not-found" on the first
 * mismatch. If `require_evidence` (see above) and evidence_ref_count is
 * 0, REJECT with "no-evidence-cited". Otherwise ACCEPT. This function
 * never returns ABSTAIN - a missing/fabricated evidence reference is
 * something this role can determine outright, not a case of irreducible
 * uncertainty. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_perceive_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote);

USDK_END_DECLS

#endif /* USDK_PERCEIVE_H */
