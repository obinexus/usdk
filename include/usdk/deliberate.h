#ifndef USDK_DELIBERATE_H
#define USDK_DELIBERATE_H

#include "usdk/plugin.h"

USDK_BEGIN_DECLS

/* usdk-deliberate: produces candidate responses/actions using a
 * pluggable backend driver - see docs/ARCHITECTURE.md.
 *
 * `cfg->data` is opaque JSON: {"driver_path": "<path to a
 * usdk-driver-<backend> module>"}. usdk_deliberate_create loads that
 * driver via usdk-ffi internally (this is the one place in this
 * repository usdk-deliberate depends on usdk-ffi - see
 * docs/PACKAGES.md's dependency table) and keeps it loaded for the
 * instance's lifetime.
 *
 * Directly callable (not only via usdk_plugin_query_v1) for the same
 * reason given in usdk/perceive.h. Not thread-safe per instance. */

USDK_EXPORT usdk_status_t USDK_CALL usdk_deliberate_create(
    const usdk_config_t* cfg, usdk_role_instance_t** out);
USDK_EXPORT void USDK_CALL usdk_deliberate_destroy(usdk_role_instance_t* inst);

/* Calls the configured driver to generate a response from `prompt` and
 * the supplied evidence, and records the resulting content digest in
 * this instance's own small self-generated-candidates history (bounded,
 * oldest evicted first) so a later usdk_deliberate_vote on a candidate
 * built from this response can recognize it as self-generated. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_deliberate_propose(
    usdk_role_instance_t* inst,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t prompt,
    usdk_owned_buffer_t* out_response);

/* Distinct check: REJECT with reason_code "not-self-generated" if
 * `candidate->proposed_response` does not match (by digest) anything
 * this instance produced via usdk_deliberate_propose - a rubber-stamp
 * vote on an arbitrary candidate is exactly what this check exists to
 * prevent (docs/ARCHITECTURE.md). Otherwise ACCEPT. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_deliberate_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote);

USDK_END_DECLS

#endif /* USDK_DELIBERATE_H */
