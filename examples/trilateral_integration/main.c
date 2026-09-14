/* The complete trilateral integration example: perceive observes raw
 * input, deliberate proposes a response via the fixture driver, all
 * three parties vote on the resulting candidate, and usdk-core applies
 * the unanimous-acceptance commit rule - see docs/CONSENSUS_PROTOCOL.md.
 * `usdk demo --scenario trilateral-consensus` (src/cli/main.c) covers
 * more edge cases (timeout, mutation) with JSON output for scripting;
 * this example is the plain, readable walkthrough referenced from
 * docs/GETTING_STARTED.md. */

#include <stdio.h>
#include <string.h>
#include "usdk/usdk.h"

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#define USDK_PLATFORM_EXT ".dll"
#define USDK_MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#define USDK_PLATFORM_EXT ".dylib"
#else
#define USDK_PLATFORM_EXT ".so"
#endif
#define USDK_MKDIR(p) mkdir(p, 0755)
#endif

static void own_exe_dir(char* out, size_t out_cap) {
#if defined(_WIN32)
    char path[1024];
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    if (n > 0 && n < sizeof(path)) {
        char* slash = strrchr(path, '\\');
        if (slash) { size_t len = (size_t)(slash - path); if (len < out_cap) { memcpy(out, path, len); out[len] = '\0'; return; } }
    }
#else
    char path[1024];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0) {
        path[n] = '\0';
        char* slash = strrchr(path, '/');
        if (slash) { size_t len = (size_t)(slash - path); if (len < out_cap) { memcpy(out, path, len); out[len] = '\0'; return; } }
    }
#endif
    if (out_cap) { out[0] = '.'; out[1] = '\0'; }
}

static const char* round_state_str(usdk_round_state_t s) {
    switch (s) {
        case USDK_ROUND_OPEN: return "OPEN";
        case USDK_ROUND_COMMITTED: return "COMMITTED";
        case USDK_ROUND_REJECTED: return "REJECTED";
        case USDK_ROUND_TIMED_OUT: return "TIMED_OUT";
        default: return "CANCELLED";
    }
}

static usdk_status_t dispatch_print(void* user_data, const usdk_candidate_t* c, const char* idempotency_key) {
    (void)user_data;
    printf("  [dispatch] committed action for candidate_id=%s: \"%.*s\"\n",
           idempotency_key, (int)c->proposed_response.len, (const char*)c->proposed_response.data);
    return USDK_OK;
}

/* Windows paths contain backslashes, the JSON escape character -
 * embedding one unescaped into the hand-built JSON config below would
 * corrupt it (the minimal parser's unrecognized-escape fallback takes
 * the following character literally, e.g. "\U" becomes "U"). Windows
 * accepts forward slashes in paths natively, so normalizing avoids
 * needing real JSON escaping. */
static void normalize_slashes(char* path) {
    for (char* p = path; *p; ++p) if (*p == '\\') *p = '/';
}

int main(void) {
    char exe_dir[1024];
    own_exe_dir(exe_dir, sizeof(exe_dir));
    char driver_path[1200];
    snprintf(driver_path, sizeof(driver_path), "%s/usdk_driver_fixture" USDK_PLATFORM_EXT, exe_dir);
    normalize_slashes(driver_path);
    char cfg_json[1300];
    int cn = snprintf(cfg_json, sizeof(cfg_json), "{\"driver_path\":\"%s\"}", driver_path);
    usdk_config_t deliberate_cfg = { { (const uint8_t*)cfg_json, (uint32_t)cn } };

    const char* verify_cfg_json = "{\"allowed_constraints\":[\"workspace:temp-only\"],\"max_uncertainty\":0.5}";
    usdk_config_t verify_cfg = { { (const uint8_t*)verify_cfg_json, (uint32_t)strlen(verify_cfg_json) } };

    usdk_role_instance_t *perceive, *deliberate, *verify;
    usdk_perceive_create(NULL, &perceive);
    if (usdk_deliberate_create(&deliberate_cfg, &deliberate) != USDK_OK) {
        fprintf(stderr, "failed to create usdk-deliberate (driver at %s)\n", driver_path);
        return 1;
    }
    usdk_verify_create(&verify_cfg, &verify);

    USDK_MKDIR("./usdk-trilateral-example-run");
    usdk_core_t* core = NULL;
    usdk_core_create("./usdk-trilateral-example-run", &core);
    usdk_core_set_dispatch_fn(core, dispatch_print, NULL);

    /* Scenario A: sufficient evidence, permitted constraint -> commits. */
    {
        printf("--- Scenario A: unanimous accept ---\n");
        usdk_buffer_t raw = { (const uint8_t*)"inbox: 3 new messages, none urgent", 35 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"draft a one-line status summary", 32 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "trilateral-example", 1, "cand-a",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() + 5000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &candidate, &round);
        usdk_vote_t vote;
        usdk_perceive_vote(perceive, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        printf("  perceive: %s\n", vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT");
        usdk_deliberate_vote(deliberate, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        printf("  deliberate: %s\n", vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT");
        usdk_verify_vote(verify, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        printf("  verify: %s\n", vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT");

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        printf("  round state: %s\n", round_state_str(state));
        usdk_core_round_destroy(round);
        usdk_owned_buffer_release(&response);
    }

    /* Scenario B: candidate asks for a constraint verify does not permit
     * -> explained refusal, nothing dispatched. */
    {
        printf("--- Scenario B: insufficient permission ---\n");
        usdk_buffer_t raw = { (const uint8_t*)"account balance query result", 29 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"propose a funds transfer", 25 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "action:transfer-funds" }}; /* not permitted */
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "trilateral-example", 2, "cand-b",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() + 5000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &candidate, &round);
        usdk_vote_t vote;
        usdk_perceive_vote(perceive, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        usdk_deliberate_vote(deliberate, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        usdk_verify_vote(verify, &candidate, &vote); usdk_core_submit_vote(core, round, &vote);
        printf("  verify: %s (%s: %s)\n", vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT",
               vote.reason_code, vote.reason_detail);

        usdk_round_state_t state;
        usdk_core_round_decide(core, round, &state);
        printf("  round state: %s (nothing dispatched)\n", round_state_str(state));
        usdk_core_round_destroy(round);
        usdk_owned_buffer_release(&response);
    }

    usdk_core_destroy(core);
    usdk_perceive_destroy(perceive);
    usdk_deliberate_destroy(deliberate);
    usdk_verify_destroy(verify);
    return 0;
}
