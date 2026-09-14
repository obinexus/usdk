#ifndef USDK_VERIFY_H
#define USDK_VERIFY_H

#include "usdk/plugin.h"

USDK_BEGIN_DECLS

/* usdk-verify: independently checks a candidate against available
 * evidence, declared constraints, and action permissions - see
 * docs/ARCHITECTURE.md.
 *
 * `cfg->data` is opaque JSON:
 * {"allowed_constraints": ["workspace:temp-only", ...],
 *  "max_uncertainty": 0.4}
 * - the permission allow-list and the evidence-sufficiency threshold
 * this instance enforces. Both are caller-supplied policy, never a
 * value this library treats as derived or proven - see
 * docs/RESEARCH_REVIEW.md section 1.3.5 for why USDK does not hardcode
 * any such threshold as a "correct" constant.
 *
 * Directly callable (not only via usdk_plugin_query_v1) for the same
 * reason given in usdk/perceive.h. Not thread-safe per instance. */

USDK_EXPORT usdk_status_t USDK_CALL usdk_verify_create(
    const usdk_config_t* cfg, usdk_role_instance_t** out);
USDK_EXPORT void USDK_CALL usdk_verify_destroy(usdk_role_instance_t* inst);

/* Distinct check: (1) every constraint in `candidate->constraints` must
 * be present in this instance's `allowed_constraints` - REJECT with
 * "constraint-not-permitted" on the first one that is not; (2) every
 * evidence_ref's `uncertainty` must be <= `max_uncertainty` - REJECT
 * with "uncertainty-exceeds-policy" on the first one that is not; (3) if
 * `evidence_ref_count` is 0, REJECT with "insufficient-evidence".
 * Otherwise ACCEPT. This is the independent permission/sufficiency gate
 * - it does not check whether the evidence is genuine (that is
 * usdk-perceive's check) or whether deliberate actually generated the
 * response (that is usdk-deliberate's check). */
USDK_EXPORT usdk_status_t USDK_CALL usdk_verify_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote);

USDK_END_DECLS

#endif /* USDK_VERIFY_H */
