#include "usdk_test.h"
#include "usdk/candidate.h"

USDK_TEST_MAIN_BEGIN()
    usdk_evidence_ref_t ev;
    memset(&ev, 0, sizeof(ev));
    strcpy(ev.evidence_id, "ev-1");
    ev.uncertainty = 0.2;

    usdk_buffer_t response = { (const uint8_t*)"hello", 5 };
    usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};

    usdk_candidate_t c1;
    usdk_status_t st = usdk_candidate_create(1, "session-a", 1, "cand-1",
                                              &ev, 1, response, constraints, 1, 1000, &c1);
    USDK_CHECK_EQ_INT(st, USDK_OK);
    USDK_CHECK(usdk_candidate_digest_matches(&c1));

    /* Same logical content, created again, must produce the same digest
     * (determinism), and a candidate with different content must
     * produce a different one (the digest is not a constant / not
     * trivially satisfied). */
    usdk_candidate_t c1_again;
    usdk_candidate_create(1, "session-a", 1, "cand-1", &ev, 1, response, constraints, 1, 1000, &c1_again);
    USDK_CHECK(memcmp(c1.content_digest, c1_again.content_digest, USDK_DIGEST_LEN) == 0);

    usdk_buffer_t different_response = { (const uint8_t*)"goodbye", 7 };
    usdk_candidate_t c2;
    usdk_candidate_create(1, "session-a", 2, "cand-2", &ev, 1, different_response, constraints, 1, 1000, &c2);
    USDK_CHECK(memcmp(c1.content_digest, c2.content_digest, USDK_DIGEST_LEN) != 0);

    /* Candidate mutation detection: directly overwrite a field after
     * creation (bypassing the type's own lack of setters, the way
     * usdk_candidate_digest_matches's own doc comment says it defends
     * against - see docs/CONSENSUS_PROTOCOL.md "Candidate mutation
     * detection") and confirm the digest no longer matches. */
    usdk_candidate_t mutated = c1;
    mutated.round_id = 999; /* changed after the digest was computed */
    USDK_CHECK(!usdk_candidate_digest_matches(&mutated));

    /* Invalid arguments are rejected, not silently accepted. */
    usdk_candidate_t bad;
    st = usdk_candidate_create(1, NULL, 1, "cand-x", &ev, 1, response, constraints, 1, 1000, &bad);
    USDK_CHECK_EQ_INT(st, USDK_ERR_INVALID_ARGUMENT);

    char too_long[USDK_ID_LEN + 10];
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    st = usdk_candidate_create(1, too_long, 1, "cand-x", &ev, 1, response, constraints, 1, 1000, &bad);
    USDK_CHECK_EQ_INT(st, USDK_ERR_INVALID_ARGUMENT);
USDK_TEST_MAIN_END()
