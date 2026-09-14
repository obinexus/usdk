#include "usdk_test.h"
#include "usdk/verify.h"

USDK_TEST_MAIN_BEGIN()
    const char* cfg_json = "{\"allowed_constraints\":[\"workspace:temp-only\",\"action:read-only\"],\"max_uncertainty\":0.4}";
    usdk_config_t cfg = { { (const uint8_t*)cfg_json, (uint32_t)strlen(cfg_json) } };
    usdk_role_instance_t* inst = NULL;
    USDK_CHECK_EQ_INT(usdk_verify_create(&cfg, &inst), USDK_OK);

    usdk_evidence_ref_t ev;
    memset(&ev, 0, sizeof(ev));
    strcpy(ev.evidence_id, "ev-1");
    usdk_buffer_t response = { (const uint8_t*)"r", 1 };

    /* Permitted constraint + acceptable uncertainty -> ACCEPT. */
    {
        ev.uncertainty = 0.1;
        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 1, "c1", &ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        USDK_CHECK_EQ_INT(usdk_verify_vote(inst, &c, &v), USDK_OK);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_ACCEPT);
        USDK_CHECK_EQ_INT(v.role, USDK_ROLE_VERIFY);
    }

    /* Disallowed constraint -> REJECT "constraint-not-permitted", checked
     * before uncertainty (this role's own distinct check order). */
    {
        ev.uncertainty = 0.1;
        usdk_constraint_t constraints[1] = {{ "action:delete-everything" }};
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 2, "c2", &ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_verify_vote(inst, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "constraint-not-permitted");
    }

    /* Permitted constraint but uncertainty above policy -> REJECT
     * "uncertainty-exceeds-policy". */
    {
        ev.uncertainty = 0.9;
        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 3, "c3", &ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_verify_vote(inst, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "uncertainty-exceeds-policy");
    }

    /* Zero evidence -> REJECT "insufficient-evidence". */
    {
        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 4, "c4", NULL, 0, response, constraints, 1, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_verify_vote(inst, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "insufficient-evidence");
    }

    usdk_verify_destroy(inst);
USDK_TEST_MAIN_END()
