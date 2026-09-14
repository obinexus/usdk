#ifndef USDK_JOURNAL_H
#define USDK_JOURNAL_H

#include "usdk/types.h"
#include "usdk/status.h"

/* Internal-only append-only round journal - see
 * docs/CONSENSUS_PROTOCOL.md "Commit records and side-effect dispatch"
 * and "Crash and restart behavior". One line per record, simple text
 * format, deliberately human-readable for debugging
 * (`usdk inspect`/`usdk doctor` read it directly). Not part of the
 * public ABI. */

typedef enum usdk_journal_record_type {
    USDK_JOURNAL_OPEN = 0,
    USDK_JOURNAL_COMMITTED,
    USDK_JOURNAL_REJECTED,
    USDK_JOURNAL_TIMED_OUT,
    USDK_JOURNAL_CANCELLED,
    USDK_JOURNAL_DISPATCHED,
    USDK_JOURNAL_DISPATCH_UNKNOWN
} usdk_journal_record_type_t;

typedef struct usdk_journal usdk_journal_t; /* opaque */

usdk_status_t usdk_journal_open(const char* runtime_dir, usdk_journal_t** out);
void usdk_journal_close(usdk_journal_t* j);

usdk_status_t usdk_journal_append(usdk_journal_t* j, usdk_journal_record_type_t type,
                                   const char* session_id, uint64_t round_id,
                                   const char* candidate_id);

typedef struct usdk_journal_record {
    usdk_journal_record_type_t type;
    char     session_id[USDK_ID_LEN];
    uint64_t round_id;
    char     candidate_id[USDK_ID_LEN];
} usdk_journal_record_t;

/* Reads every record currently in the journal into `out` (capacity
 * `out_cap`), oldest first. Returns USDK_ERR_OUT_OF_MEMORY-equivalent
 * behavior by simply truncating at `out_cap` (sets *out_count to
 * out_cap) rather than failing - the journal is meant to stay small
 * (one open runtime_dir's worth of rounds), and callers needing more
 * than a bounded scan is not a case this reference implementation
 * targets. */
usdk_status_t usdk_journal_read_all(usdk_journal_t* j, usdk_journal_record_t* out,
                                     uint32_t out_cap, uint32_t* out_count);

/* Highest round_id previously OPENed for `session_id`, or 0 if none -
 * used to enforce docs/CONSENSUS_PROTOCOL.md "Stale rounds and replay
 * rejection" across process restarts, not just within one usdk_core_t's
 * in-memory lifetime. */
uint64_t usdk_journal_highest_round_id(usdk_journal_t* j, const char* session_id);

#endif /* USDK_JOURNAL_H */
