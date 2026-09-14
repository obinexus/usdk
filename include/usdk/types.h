#ifndef USDK_TYPES_H
#define USDK_TYPES_H

#include <stdint.h>
#include "usdk/platform.h"

USDK_BEGIN_DECLS

/* Fixed capacity for every opaque/caller-facing identifier string in the
 * public ABI (session_id, candidate_id, party_id, evidence_id,
 * constraint_id, module/capability names). One constant, one place, so
 * every struct that embeds an id agrees on its size - see docs/ABI.md
 * "Fixed-width types, buffers, and ownership". */
#define USDK_ID_LEN 64

#define USDK_DIGEST_LEN 32 /* SHA-256 */

typedef enum usdk_role {
    USDK_ROLE_UNSPECIFIED = 0,
    USDK_ROLE_PERCEIVE    = 1,
    USDK_ROLE_DELIBERATE  = 2,
    USDK_ROLE_VERIFY      = 3,
    USDK_ROLE_DRIVER      = 4
} usdk_role_t;

/* Borrowed, read-only view: valid only for the duration of the call it
 * was passed to, unless that call's own documentation says otherwise.
 * Never owns `data`. */
typedef struct usdk_buffer {
    const uint8_t* data;
    uint32_t       len;
} usdk_buffer_t;

/* An allocation the caller owns and must release with the specific
 * paired _release function named in the API that returned it - see
 * docs/ABI.md "Fixed-width types, buffers, and ownership". */
typedef struct usdk_owned_buffer {
    uint8_t* data;
    uint32_t len;
    uint32_t cap;
} usdk_owned_buffer_t;

USDK_EXPORT void USDK_CALL usdk_owned_buffer_release(usdk_owned_buffer_t* buf);

/* Monotonic clock used for every deadline/timestamp in this ABI - never
 * wall-clock, so it is immune to clock adjustment. Thread-safe, pure
 * with respect to any USDK state. Declared here (usdk-contracts) rather
 * than usdk/core.h so every role library can time-stamp its own votes
 * without linking usdk-core - see docs/PACKAGES.md's dependency table. */
USDK_EXPORT int64_t USDK_CALL usdk_monotonic_ns(void);

USDK_END_DECLS

#endif /* USDK_TYPES_H */
