/* Standalone example: usdk-verify used entirely on its own, linked
 * directly - no usdk-perceive, no usdk-deliberate. Demonstrates its
 * independent permission/sufficiency gate (docs/ARCHITECTURE.md): a
 * permitted constraint with low-uncertainty evidence is accepted; a
 * disallowed constraint, and separately over-uncertain evidence, are
 * each rejected for their own distinct reason. */

#include <stdio.h>
#include <string.h>
#include "usdk/verify.h"
#include "usdk/candidate.h"

static void print_vote(const char* label, const usdk_vote_t* vote) {
    printf("%s: %s (%s: %s)\n", label, vote->verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT",
           vote->reason_code, vote->reason_detail);
}

int main(void) {
    const char* cfg_json = "{\"allowed_constraints\":[\"workspace:temp-only\"],\"max_uncertainty\":0.4}";
    usdk_config_t cfg = { { (const uint8_t*)cfg_json, (uint32_t)strlen(cfg_json) } };

    usdk_role_instance_t* verify = NULL;
    if (usdk_verify_create(&cfg, &verify) != USDK_OK) {
        fprintf(stderr, "usdk_verify_create failed\n");
        return 1;
    }

    usdk_evidence_ref_t low_uncertainty_ev;
    memset(&low_uncertainty_ev, 0, sizeof(low_uncertainty_ev));
    strcpy(low_uncertainty_ev.evidence_id, "ev-1");
    low_uncertainty_ev.uncertainty = 0.1;

    usdk_buffer_t response = { (const uint8_t*)"write a note to the temp workspace", 35 };

    /* Permitted constraint, acceptable uncertainty -> ACCEPT. */
    {
        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "verify-example", 1, "cand-ok",
                               &low_uncertainty_ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &candidate);
        usdk_vote_t vote;
        usdk_verify_vote(verify, &candidate, &vote);
        print_vote("permitted constraint + low uncertainty", &vote);
    }

    /* Disallowed constraint -> REJECT with "constraint-not-permitted". */
    {
        usdk_constraint_t constraints[1] = {{ "action:delete-all" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "verify-example", 2, "cand-forbidden-constraint",
                               &low_uncertainty_ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &candidate);
        usdk_vote_t vote;
        usdk_verify_vote(verify, &candidate, &vote);
        print_vote("disallowed constraint", &vote);
    }

    /* Permitted constraint but evidence too uncertain -> REJECT with
     * "uncertainty-exceeds-policy". */
    {
        usdk_evidence_ref_t high_uncertainty_ev = low_uncertainty_ev;
        high_uncertainty_ev.uncertainty = 0.9;
        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "verify-example", 3, "cand-too-uncertain",
                               &high_uncertainty_ev, 1, response, constraints, 1, 0x7fffffffffffffffLL, &candidate);
        usdk_vote_t vote;
        usdk_verify_vote(verify, &candidate, &vote);
        print_vote("permitted constraint + high uncertainty", &vote);
    }

    usdk_verify_destroy(verify);
    return 0;
}
