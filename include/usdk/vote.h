#ifndef USDK_VOTE_H
#define USDK_VOTE_H

#include "usdk/types.h"

USDK_BEGIN_DECLS

typedef enum usdk_verdict {
    USDK_VERDICT_ACCEPT  = 0,
    USDK_VERDICT_REJECT  = 1,
    USDK_VERDICT_ABSTAIN = 2
} usdk_verdict_t;

/* One party's evaluation of one candidate. See
 * docs/CONSENSUS_PROTOCOL.md "Verdict vocabulary and the commit rule". */
typedef struct usdk_vote {
    uint32_t       struct_size; /* sizeof(usdk_vote_t) as built */
    char           party_id[USDK_ID_LEN];
    usdk_role_t    role;
    usdk_verdict_t verdict;
    char           reason_code[USDK_ID_LEN];   /* short, machine-readable,
                                                 * e.g. "evidence-not-found" */
    char           reason_detail[256];         /* human-readable */
    uint8_t        candidate_digest_seen[USDK_DIGEST_LEN]; /* the digest of
                                                 * the candidate this vote
                                                 * was actually cast on -
                                                 * see "Candidate mutation
                                                 * detection". */
    int64_t        voted_at_ns; /* usdk_monotonic_ns() at cast time */
} usdk_vote_t;

USDK_END_DECLS

#endif /* USDK_VOTE_H */
