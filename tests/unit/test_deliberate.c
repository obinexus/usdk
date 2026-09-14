#include "usdk_test.h"
#include "usdk/deliberate.h"

#ifndef USDK_TEST_DRIVER_FIXTURE_PATH
#error "USDK_TEST_DRIVER_FIXTURE_PATH must be defined by the build"
#endif

USDK_TEST_MAIN_BEGIN()
    char cfg_json[900];
    int n = snprintf(cfg_json, sizeof(cfg_json), "{\"driver_path\":\"%s\"}", USDK_TEST_DRIVER_FIXTURE_PATH);
    usdk_config_t cfg = { { (const uint8_t*)cfg_json, (uint32_t)n } };

    usdk_role_instance_t* inst = NULL;
    USDK_CHECK_EQ_INT(usdk_deliberate_create(&cfg, &inst), USDK_OK);

    /* A missing driver_path (config with no such key) is rejected, not
     * silently defaulted. */
    {
        const char* bad_cfg_json = "{}";
        usdk_config_t bad_cfg = { { (const uint8_t*)bad_cfg_json, (uint32_t)strlen(bad_cfg_json) } };
        usdk_role_instance_t* bad_inst = NULL;
        USDK_CHECK_EQ_INT(usdk_deliberate_create(&bad_cfg, &bad_inst), USDK_ERR_INVALID_ARGUMENT);
    }

    usdk_buffer_t prompt = { (const uint8_t*)"say something", 13 };
    usdk_owned_buffer_t response = {0};
    USDK_CHECK_EQ_INT(usdk_deliberate_propose(inst, NULL, 0, prompt, &response), USDK_OK);
    USDK_CHECK(response.len > 0);
    /* Fixture responses must be clearly labeled - see
     * docs/IMPLEMENTATION_STATUS.md "Vertical slice" and
     * src/driver_fixture/driver_fixture.c. */
    USDK_CHECK(memcmp(response.data, "[usdk-driver-fixture]", 21) == 0); /* 21 bytes - not the literal's NUL */

    /* Voting on the exact candidate built from that response -> ACCEPT. */
    {
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 1, "c1", NULL, 0,
                               (usdk_buffer_t){ response.data, response.len }, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        USDK_CHECK_EQ_INT(usdk_deliberate_vote(inst, &c, &v), USDK_OK);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_ACCEPT);
        USDK_CHECK_EQ_INT(v.role, USDK_ROLE_DELIBERATE);
    }

    /* Voting on a candidate whose response this instance never proposed
     * -> REJECT "not-self-generated" - not a rubber stamp. */
    {
        usdk_buffer_t foreign = { (const uint8_t*)"an externally-authored response", 32 };
        usdk_candidate_t c;
        usdk_candidate_create(1, "s", 2, "c2", NULL, 0, foreign, NULL, 0, 0x7fffffffffffffffLL, &c);
        usdk_vote_t v;
        usdk_deliberate_vote(inst, &c, &v);
        USDK_CHECK_EQ_INT(v.verdict, USDK_VERDICT_REJECT);
        USDK_CHECK_STR_EQ(v.reason_code, "not-self-generated");
    }

    usdk_owned_buffer_release(&response);
    usdk_deliberate_destroy(inst);
USDK_TEST_MAIN_END()
