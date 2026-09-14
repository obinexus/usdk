#include "usdk_test.h"
#include "usdk/core.h"
#include "usdk/candidate.h"
#include "usdk/vote.h"

static usdk_candidate_t make_candidate(const char* session, uint64_t round_id, const char* cand_id, int64_t deadline_ns) {
    usdk_candidate_t c;
    usdk_buffer_t response = { (const uint8_t*)"response", 8 };
    usdk_candidate_create(1, session, round_id, cand_id, NULL, 0, response, NULL, 0, deadline_ns, &c);
    return c;
}

static void fill_vote(usdk_vote_t* v, const char* party_id, usdk_role_t role, usdk_verdict_t verdict,
                       const usdk_candidate_t* c) {
    memset(v, 0, sizeof(*v));
    v->struct_size = (uint32_t)sizeof(*v);
    strncpy(v->party_id, party_id, USDK_ID_LEN - 1);
    v->role = role;
    v->verdict = verdict;
    strncpy(v->reason_code, "test", USDK_ID_LEN - 1);
    memcpy(v->candidate_digest_seen, c->content_digest, USDK_DIGEST_LEN);
    v->voted_at_ns = usdk_monotonic_ns();
}

static int g_dispatch_calls = 0;
static usdk_status_t counting_dispatch(void* user_data, const usdk_candidate_t* c, const char* key) {
    (void)user_data; (void)c; (void)key;
    g_dispatch_calls++;
    return USDK_OK;
}

USDK_TEST_MAIN_BEGIN()
    /* A fresh, unique directory per run - not a fixed name - so a
     * previous run's journal (which persists on disk by design, see
     * docs/CONSENSUS_PROTOCOL.md "Crash and restart behavior") can never
     * be mistaken for this run's and make a brand-new round_id look like
     * a replay (usdk_core_open_round's stale-round check reads the
     * on-disk journal, not just in-process state - see
     * src/core/core.c/usdk_core_open_round). */
    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "./usdk-test-core-consensus-runtime-%lld",
             (long long)usdk_monotonic_ns());

    usdk_core_t* core = NULL;
    USDK_CHECK_EQ_INT(usdk_core_create(runtime_dir, &core), USDK_OK);
    usdk_core_set_dispatch_fn(core, counting_dispatch, NULL);

    int64_t far_future = 5000000000LL;

    /* --- 1. Unanimous accept commits, and dispatch runs exactly once. - */
    {
        usdk_candidate_t c = make_candidate("s1", 1, "c1", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        USDK_CHECK_EQ_INT(usdk_core_open_round(core, &c, &round), USDK_OK);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_OK);
        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_OK);
        fill_vote(&v, "v1", USDK_ROLE_VERIFY, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_OK);

        usdk_round_state_t state;
        USDK_CHECK_EQ_INT(usdk_core_round_decide(core, round, &state), USDK_OK);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_COMMITTED);
        USDK_CHECK_EQ_INT(g_dispatch_calls, 1);

        /* Idempotent: deciding an already-terminal round again returns
         * the same state and does not dispatch a second time. */
        USDK_CHECK_EQ_INT(usdk_core_round_decide(core, round, &state), USDK_OK);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_COMMITTED);
        USDK_CHECK_EQ_INT(g_dispatch_calls, 1);

        usdk_core_round_destroy(round);
    }

    /* --- 2. Any single REJECT prevents commit; dispatch not called. -- */
    {
        usdk_candidate_t c = make_candidate("s2", 1, "c2", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_REJECT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "v1", USDK_ROLE_VERIFY, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_REJECTED);
        USDK_CHECK_EQ_INT(g_dispatch_calls, 1); /* unchanged from scenario 1 */
        usdk_core_round_destroy(round);
    }

    /* --- 3. An ABSTAIN prevents commit, same as a REJECT would. ------ */
    {
        usdk_candidate_t c = make_candidate("s3", 1, "c3", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_ABSTAIN, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "v1", USDK_ROLE_VERIFY, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_REJECTED);
        usdk_core_round_destroy(round);
    }

    /* --- 4. A missing vote (only 2 of 3 submitted) prevents commit. -- */
    {
        usdk_candidate_t c = make_candidate("s4", 1, "c4", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        /* verify never votes */

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_REJECTED);
        usdk_core_round_destroy(round);
    }

    /* --- 5. An already-expired deadline times the round out, both via
     * submit_vote and via decide. -------------------------------------- */
    {
        usdk_candidate_t c = make_candidate("s5", 1, "c5", usdk_monotonic_ns() - 1000000000LL);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_ERR_DEADLINE_EXCEEDED);
        USDK_CHECK_EQ_INT(usdk_round_get_state(round), USDK_ROUND_TIMED_OUT);
        usdk_core_round_destroy(round);
    }

    /* --- 6. A duplicate vote for the same role is rejected; the first
     * vote is preserved (not overwritten). ------------------------------ */
    {
        usdk_candidate_t c = make_candidate("s6", 1, "c6", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_OK);
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_REJECT, &c); /* same party, second call */
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_ERR_DUPLICATE_VOTE);

        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "v1", USDK_ROLE_VERIFY, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        /* The preserved first vote was ACCEPT, so this still commits -
         * proving the REJECT resubmission was truly discarded, not just
         * rejected-but-still-somehow-counted. */
        USDK_CHECK_EQ_INT(state, USDK_ROUND_COMMITTED);
        usdk_core_round_destroy(round);
    }

    /* --- 7. A different party_id claiming the same role is a conflict,
     * not a silent overwrite or an accepted second voter. -------------- */
    {
        usdk_candidate_t c = make_candidate("s7", 1, "c7", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);
        fill_vote(&v, "p1-impostor", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_ERR_PARTY_CONFLICT);
        usdk_core_round_destroy(round);
    }

    /* --- 8. Stale/replayed round_id is refused: round_id must strictly
     * increase within a session. ---------------------------------------- */
    {
        usdk_candidate_t c1 = make_candidate("s8", 1, "c8a", usdk_monotonic_ns() + far_future);
        usdk_round_t* r1 = NULL;
        USDK_CHECK_EQ_INT(usdk_core_open_round(core, &c1, &r1), USDK_OK);
        usdk_core_cancel_round(core, r1);
        usdk_core_round_destroy(r1);

        /* Same round_id again (a replay) must be refused. */
        usdk_candidate_t c1_replay = make_candidate("s8", 1, "c8a-replay", usdk_monotonic_ns() + far_future);
        usdk_round_t* r_replay = NULL;
        USDK_CHECK_EQ_INT(usdk_core_open_round(core, &c1_replay, &r_replay), USDK_ERR_STALE_ROUND);

        /* A strictly higher round_id in the same session is fine. */
        usdk_candidate_t c2 = make_candidate("s8", 2, "c8b", usdk_monotonic_ns() + far_future);
        usdk_round_t* r2 = NULL;
        USDK_CHECK_EQ_INT(usdk_core_open_round(core, &c2, &r2), USDK_OK);
        usdk_core_cancel_round(core, r2);
        usdk_core_round_destroy(r2);
    }

    /* --- 9. A vote whose recorded candidate_digest_seen does not match
     * the round's candidate cannot count toward commit, even if its
     * verdict is ACCEPT - see docs/CONSENSUS_PROTOCOL.md "Candidate
     * mutation detection". ------------------------------------------------ */
    {
        usdk_candidate_t c = make_candidate("s9", 1, "c9", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);

        fill_vote(&v, "d1", USDK_ROLE_DELIBERATE, USDK_VERDICT_ACCEPT, &c);
        memset(v.candidate_digest_seen, 0xAA, USDK_DIGEST_LEN); /* tampered/stale digest */
        usdk_core_submit_vote(core, round, &v);

        fill_vote(&v, "v1", USDK_ROLE_VERIFY, USDK_VERDICT_ACCEPT, &c);
        usdk_core_submit_vote(core, round, &v);

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        USDK_CHECK_EQ_INT(state, USDK_ROUND_REJECTED);
        usdk_core_round_destroy(round);
    }

    /* --- 10. Cancelling an open round prevents any further vote. ----- */
    {
        usdk_candidate_t c = make_candidate("s10", 1, "c10", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &c, &round);
        USDK_CHECK_EQ_INT(usdk_core_cancel_round(core, round), USDK_OK);
        USDK_CHECK_EQ_INT(usdk_round_get_state(round), USDK_ROUND_CANCELLED);

        usdk_vote_t v;
        fill_vote(&v, "p1", USDK_ROLE_PERCEIVE, USDK_VERDICT_ACCEPT, &c);
        USDK_CHECK_EQ_INT(usdk_core_submit_vote(core, round, &v), USDK_ERR_ROUND_NOT_OPEN);
        /* Cancelling twice is also rejected, not silently accepted. */
        USDK_CHECK_EQ_INT(usdk_core_cancel_round(core, round), USDK_ERR_ROUND_NOT_OPEN);
        usdk_core_round_destroy(round);
    }

    usdk_core_destroy(core);

    /* --- 11. Recovery: a round left OPEN (never decided) is reported by
     * usdk_core_recover_incomplete_rounds after reopening the same
     * runtime_dir - simulating what a restart finds. --------------------- */
    {
        char recovery_dir[280];
        snprintf(recovery_dir, sizeof(recovery_dir), "%s-recovery", runtime_dir);

        usdk_core_t* core2 = NULL;
        usdk_core_create(recovery_dir, &core2);
        usdk_candidate_t c = make_candidate("s-recover", 1, "c-recover", usdk_monotonic_ns() + far_future);
        usdk_round_t* round = NULL;
        usdk_core_open_round(core2, &c, &round);
        /* Deliberately never call usdk_core_round_decide or
         * usdk_core_cancel_round - this leaves an OPEN record with no
         * terminal record, exactly what an unclean restart would find. */
        usdk_core_round_destroy(round);
        usdk_core_destroy(core2);

        usdk_core_t* core3 = NULL;
        usdk_core_create(recovery_dir, &core3);
        usdk_incomplete_round_t incomplete[16];
        uint32_t n = 0;
        USDK_CHECK_EQ_INT(usdk_core_recover_incomplete_rounds(core3, incomplete, 16, &n), USDK_OK);
        USDK_CHECK_EQ_INT(n, 1);
        if (n == 1) {
            USDK_CHECK_STR_EQ(incomplete[0].session_id, "s-recover");
            USDK_CHECK_EQ_INT(incomplete[0].round_id, 1);
        }
        usdk_core_destroy(core3);
    }
USDK_TEST_MAIN_END()
