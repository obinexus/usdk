#ifndef USDK_CORE_H
#define USDK_CORE_H

#include "usdk/types.h"
#include "usdk/status.h"
#include "usdk/candidate.h"
#include "usdk/vote.h"

USDK_BEGIN_DECLS

/* usdk_monotonic_ns() and usdk_owned_buffer_release() are declared in
 * usdk/types.h (usdk-contracts), not here, so every role library can use
 * them without linking usdk-core - see docs/PACKAGES.md. */

typedef struct usdk_core usdk_core_t;   /* opaque - session/journal state */
typedef struct usdk_round usdk_round_t; /* opaque - one open/decided round */

typedef enum usdk_round_state {
    USDK_ROUND_OPEN = 0,
    USDK_ROUND_COMMITTED,
    USDK_ROUND_REJECTED,
    USDK_ROUND_TIMED_OUT,
    USDK_ROUND_CANCELLED
} usdk_round_state_t;

/* `runtime_dir` holds the append-only round journal
 * (docs/CONSENSUS_PROTOCOL.md "Commit records and side-effect dispatch")
 * - must be a writable directory; created if it does not exist. Not
 * thread-safe. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_create(const char* runtime_dir, usdk_core_t** out);
USDK_EXPORT void USDK_CALL usdk_core_destroy(usdk_core_t* core);

/* Called once, synchronously, on unanimous commit, with the committed
 * candidate's candidate_id as `idempotency_key`. See
 * docs/CONSENSUS_PROTOCOL.md "Idempotency requirements for tool
 * execution": this function MUST make repeated invocation with the same
 * key converge to the same observable end state. Returning a non-OK
 * status marks the round's dispatch outcome DISPATCH_UNKNOWN in the
 * journal, not DISPATCHED - usdk-core cannot distinguish "the action
 * definitely did not happen" from "it may have partially happened before
 * failing," so it does not guess. */
typedef usdk_status_t (USDK_CALL *usdk_dispatch_fn_t)(
    void* user_data,
    const usdk_candidate_t* committed_candidate,
    const char* idempotency_key);

USDK_EXPORT void USDK_CALL usdk_core_set_dispatch_fn(
    usdk_core_t* core, usdk_dispatch_fn_t fn, void* user_data);

/* Opens a new round for `candidate`. Fails USDK_ERR_STALE_ROUND if
 * `candidate->round_id` is not strictly greater than the highest
 * round_id already opened for `candidate->session_id` in this core's
 * lifetime - see docs/CONSENSUS_PROTOCOL.md "Stale rounds and replay
 * rejection". Appends an OPEN journal record before returning success. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_open_round(
    usdk_core_t* core, const usdk_candidate_t* candidate, usdk_round_t** out_round);

/* Records `vote` into the round's slot for `vote->role`. Fails
 * USDK_ERR_DUPLICATE_VOTE if that role's slot is already filled (even by
 * an identical vote), USDK_ERR_PARTY_CONFLICT if `vote->party_id` differs
 * from a party_id already recorded for that role this round,
 * USDK_ERR_ROUND_NOT_OPEN if the round already left the OPEN state, and
 * USDK_ERR_DEADLINE_EXCEEDED if `usdk_monotonic_ns()` is already past
 * `candidate->deadline_ns` (the round is moved to TIMED_OUT as a side
 * effect of that check). */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_submit_vote(
    usdk_core_t* core, usdk_round_t* round, const usdk_vote_t* vote);

/* Applies the commit rule (docs/CONSENSUS_PROTOCOL.md): moves an OPEN
 * round to COMMITTED (and invokes the dispatch function, if set, with
 * the round's candidate and its candidate_id as idempotency key),
 * REJECTED, or TIMED_OUT, and writes the terminal journal record(s). A
 * round already in a terminal state returns that same state again
 * without re-invoking dispatch. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_round_decide(
    usdk_core_t* core, usdk_round_t* round, usdk_round_state_t* out_state);

/* Moves an OPEN round to CANCELLED; dispatches nothing. A round not in
 * OPEN state returns USDK_ERR_ROUND_NOT_OPEN. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_cancel_round(usdk_core_t* core, usdk_round_t* round);

USDK_EXPORT void USDK_CALL usdk_core_round_destroy(usdk_round_t* round);

USDK_EXPORT usdk_round_state_t USDK_CALL usdk_round_get_state(const usdk_round_t* round);

typedef struct usdk_incomplete_round {
    char     session_id[USDK_ID_LEN];
    uint64_t round_id;
    char     candidate_id[USDK_ID_LEN];
    int      dispatch_outcome_unknown; /* 1: a COMMITTED record exists with
                                         * no matching DISPATCHED/
                                         * DISPATCH_UNKNOWN terminal record -
                                         * see docs/CONSENSUS_PROTOCOL.md
                                         * "Crash and restart behavior". */
} usdk_incomplete_round_t;

/* Scans this core's journal for rounds with no terminal record (or a
 * COMMITTED record with no matching dispatch-outcome record) - see
 * docs/CONSENSUS_PROTOCOL.md "Crash and restart behavior". Never
 * re-dispatches anything itself. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_core_recover_incomplete_rounds(
    usdk_core_t* core,
    usdk_incomplete_round_t* out, uint32_t out_cap, uint32_t* out_count);

USDK_END_DECLS

#endif /* USDK_CORE_H */
