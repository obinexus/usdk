#ifndef USDK_CANDIDATE_H
#define USDK_CANDIDATE_H

#include "usdk/types.h"
#include "usdk/status.h"

USDK_BEGIN_DECLS

/* A reference to evidence usdk-perceive produced - never a raw pointer
 * into perceive's internal state, never the evidence payload itself.
 * See docs/CONSENSUS_PROTOCOL.md "The candidate" and docs/ABI.md
 * "Serialized messages vs. in-process structures". */
typedef struct usdk_evidence_ref {
    char    evidence_id[USDK_ID_LEN];
    uint8_t evidence_digest[USDK_DIGEST_LEN];
    double  uncertainty; /* caller-supplied, in [0,1]; not validated to be
                           * in range by this struct - usdk-perceive's own
                           * vote is where an out-of-range value is caught. */
} usdk_evidence_ref_t;

typedef struct usdk_constraint {
    char constraint_id[USDK_ID_LEN]; /* e.g. "workspace:temp-only" */
} usdk_constraint_t;

/* Immutable once created by usdk_candidate_create: every field is set at
 * construction and this header declares no setter. See
 * docs/CONSENSUS_PROTOCOL.md "The candidate" for the full field-by-field
 * rationale. */
typedef struct usdk_candidate {
    uint32_t struct_size; /* sizeof(usdk_candidate_t) as built - callers
                            * across a module boundary check this first. */
    uint32_t protocol_version;
    uint32_t policy_version;
    char     session_id[USDK_ID_LEN];
    uint64_t round_id;
    char     candidate_id[USDK_ID_LEN];
    uint8_t  content_digest[USDK_DIGEST_LEN]; /* set by usdk_candidate_create,
                                                * never by the caller. */

    const usdk_evidence_ref_t* evidence_refs; /* borrowed - see note below */
    uint32_t                   evidence_ref_count;

    usdk_buffer_t proposed_response; /* borrowed */

    const usdk_constraint_t* constraints; /* borrowed */
    uint32_t                 constraint_count;

    int64_t deadline_ns; /* absolute value on the same monotonic clock as
                           * usdk_monotonic_ns() - not wall-clock. */
} usdk_candidate_t;

#define USDK_PROTOCOL_VERSION_V1 1u

/* Computes content_digest over every field except struct_size and
 * content_digest itself (see include/usdk/wire.h for the exact canonical
 * encoding) and fills it in. `evidence_refs`/`proposed_response.data`/
 * `constraints` are borrowed by the resulting candidate for as long as
 * the candidate is used - the caller must keep them alive at least that
 * long; usdk_candidate_create does not copy them (this keeps the
 * candidate cheap to create for every round without an allocator
 * dependency in usdk-contracts). Returns USDK_ERR_INVALID_ARGUMENT if
 * `out` is NULL or a required id string does not fit USDK_ID_LEN
 * (including its NUL terminator). */
USDK_EXPORT usdk_status_t USDK_CALL usdk_candidate_create(
    uint32_t policy_version,
    const char* session_id,
    uint64_t round_id,
    const char* candidate_id,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t proposed_response,
    const usdk_constraint_t* constraints, uint32_t constraint_count,
    int64_t deadline_ns,
    usdk_candidate_t* out);

/* Recomputes the canonical digest over `c` and compares it to
 * `c->content_digest`. Used by usdk-core to detect an in-memory
 * candidate altered after a vote was cast - see
 * docs/CONSENSUS_PROTOCOL.md "Candidate mutation detection". Returns 1
 * if the digest still matches (unmodified), 0 if it does not, and does
 * not distinguish further reasons (that is the caller's job). */
USDK_EXPORT int USDK_CALL usdk_candidate_digest_matches(const usdk_candidate_t* c);

USDK_END_DECLS

#endif /* USDK_CANDIDATE_H */
