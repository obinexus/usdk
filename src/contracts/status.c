#include "usdk/status.h"

const char* usdk_status_string(usdk_status_t status) {
    switch (status) {
        case USDK_OK: return "ok";
        case USDK_ERR_INVALID_ARGUMENT: return "invalid argument";
        case USDK_ERR_ABI_VERSION_MISMATCH: return "ABI version mismatch";
        case USDK_ERR_STRUCT_SIZE_MISMATCH: return "struct size mismatch";
        case USDK_ERR_CAPABILITY_NOT_FOUND: return "capability not found";
        case USDK_ERR_MODULE_LOAD_FAILED: return "module load failed";
        case USDK_ERR_MODULE_ENTRY_NOT_FOUND: return "module entry point not found";
        case USDK_ERR_DEPENDENCY_CYCLE: return "dependency cycle detected";
        case USDK_ERR_DEPENDENCY_UNRESOLVED: return "dependency unresolved";
        case USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE: return "dependency version incompatible";
        case USDK_ERR_STALE_ROUND: return "stale or replayed round";
        case USDK_ERR_DUPLICATE_VOTE: return "duplicate vote";
        case USDK_ERR_PARTY_CONFLICT: return "party identity conflict";
        case USDK_ERR_CANDIDATE_MUTATED: return "candidate mutated after vote";
        case USDK_ERR_DEADLINE_EXCEEDED: return "deadline exceeded";
        case USDK_ERR_ROUND_NOT_OPEN: return "round not open";
        case USDK_ERR_OUT_OF_MEMORY: return "out of memory";
        case USDK_ERR_IO: return "I/O error";
        case USDK_ERR_NOT_IMPLEMENTED: return "not implemented";
        default: return "unknown status";
    }
}
