#ifndef USDK_STATUS_H
#define USDK_STATUS_H

#include "usdk/platform.h"

USDK_BEGIN_DECLS

typedef enum usdk_status {
    USDK_OK = 0,
    USDK_ERR_INVALID_ARGUMENT,
    USDK_ERR_ABI_VERSION_MISMATCH,
    USDK_ERR_STRUCT_SIZE_MISMATCH,
    USDK_ERR_CAPABILITY_NOT_FOUND,
    USDK_ERR_MODULE_LOAD_FAILED,
    USDK_ERR_MODULE_ENTRY_NOT_FOUND,
    USDK_ERR_DEPENDENCY_CYCLE,
    USDK_ERR_DEPENDENCY_UNRESOLVED,
    USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE,
    USDK_ERR_STALE_ROUND,
    USDK_ERR_DUPLICATE_VOTE,
    USDK_ERR_PARTY_CONFLICT,
    USDK_ERR_CANDIDATE_MUTATED,
    USDK_ERR_DEADLINE_EXCEEDED,
    USDK_ERR_ROUND_NOT_OPEN,
    USDK_ERR_OUT_OF_MEMORY,
    USDK_ERR_IO,
    USDK_ERR_NOT_IMPLEMENTED
} usdk_status_t;

/* Pure, thread-safe: returns a pointer to a static string literal, never
 * allocated, never NULL (an unrecognized value still returns a fixed
 * "unknown status" string rather than crashing). */
USDK_EXPORT const char* USDK_CALL usdk_status_string(usdk_status_t status);

USDK_END_DECLS

#endif /* USDK_STATUS_H */
