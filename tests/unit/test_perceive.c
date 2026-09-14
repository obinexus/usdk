#include "usdk_test.h"
#include "usdk/perceive.h"

USDK_TEST_MAIN_BEGIN()
    usdk_role_instance_t* inst = NULL;
    USDK_CHECK_EQ_INT(usdk_perceive_create(NULL, &inst), USDK_OK);

    usdk_buffer_t raw = { (const uint8_t*)"reading: 42", 11 };
    usdk_evidence_ref_t ref;
    USDK_CHECK_EQ_INT(usdk_perceive_observe(inst, raw, 0.3, &ref), USDK_OK);
    USDK_CHECK(strlen(ref.evidence_id) > 0);

    /* Vote on a candidate citing the real evidence -> ACCEPT. */
    {
        usdk_buffer_t response = { (const uint8_t*)"r", 1 };
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 1, "c1", &ref, 1, response, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        USDK_CHECK_EQ_INT(usdk_perceive_vote(inst, &c, &v), USDK_OK);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_ACCEPT);
        USDK_CHECK_EQ_INT(v.role, USDK_ROLE_PERCEIVE);
    }

    /* Vote on a candidate citing evidence this instance never observed
     * -> REJECT, not ABSTAIN (this role can determine this outright). */
    {
        usdk_evidence_ref_t fake;
        memset(&fake, 0, sizeof(fake));
        strcpy(fake.evidence_id, "ev-fabricated");
        usdk_buffer_t response = { (const uint8_t*)"r", 1 };
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 2, "c2", &fake, 1, response, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        USDK_CHECK_EQ_INT(usdk_perceive_vote(inst, &c, &v), USDK_OK);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "evidence-not-found");
    }

    /* Default config requires at least one evidence_ref. */
    {
        usdk_buffer_t response = { (const uint8_t*)"r", 1 };
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 3, "c3", NULL, 0, response, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_perceive_vote(inst, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "no-evidence-cited");
    }

    usdk_perceive_destroy(inst);

    /* require_evidence:false permits zero evidence_refs. */
    {
        const char* cfg_json = "{\"require_evidence\":false}";
        usdk_config_t cfg = { { (const uint8_t*)cfg_json, (uint32_t)strlen(cfg_json) } };
        usdk_role_instance_t* inst2 = NULL;
        usdk_perceive_create(&cfg, &inst2);

        usdk_buffer_t response = { (const uint8_t*)"r", 1 };
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 1, "c4", NULL, 0, response, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_perceive_vote(inst2, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_ACCEPT);

        usdk_perceive_destroy(inst2);
    }
USDK_TEST_MAIN_END()
