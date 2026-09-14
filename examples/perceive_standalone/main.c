/* Standalone example: usdk-perceive used entirely on its own, linked
 * directly - no usdk-deliberate, no usdk-verify, no dynamic loading.
 * Demonstrates the role's full lifecycle: observe two pieces of raw
 * input, build a candidate that cites one of them plus one FABRICATED
 * evidence reference perceive never produced, and show usdk_perceive_vote
 * catching that fabrication - the "distinct check" this role performs
 * (docs/ARCHITECTURE.md). */

#include <stdio.h>
#include <string.h>
#include "usdk/perceive.h"
#include "usdk/candidate.h"

int main(void) {
    usdk_role_instance_t* perceive = NULL;
    if (usdk_perceive_create(NULL, &perceive) != USDK_OK) {
        fprintf(stderr, "usdk_perceive_create failed\n");
        return 1;
    }

    usdk_buffer_t raw1 = { (const uint8_t*)"door sensor: closed", 20 };
    usdk_evidence_ref_t ref1;
    usdk_perceive_observe(perceive, raw1, 0.05, &ref1);
    printf("observed evidence_id=%s uncertainty=%.2f\n", ref1.evidence_id, ref1.uncertainty);

    /* A candidate that cites the real evidence_ref above should be
     * accepted. */
    {
        usdk_buffer_t response = { (const uint8_t*)"the door is closed", 19 };
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "perceive-example", 1, "cand-real-evidence",
                               &ref1, 1, response, NULL, 0, 0x7fffffffffffffffLL, &candidate);

        usdk_vote_t vote;
        usdk_perceive_vote(perceive, &candidate, &vote);
        printf("vote on real evidence: %s (%s)\n",
               vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT", vote.reason_code);
    }

    /* A candidate citing a fabricated evidence_ref (never returned by
     * usdk_perceive_observe) must be rejected. */
    {
        usdk_evidence_ref_t fabricated;
        memset(&fabricated, 0, sizeof(fabricated));
        strcpy(fabricated.evidence_id, "ev-does-not-exist");
        fabricated.uncertainty = 0.0;

        usdk_buffer_t response = { (const uint8_t*)"the vault is unlocked", 22 };
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "perceive-example", 2, "cand-fabricated-evidence",
                               &fabricated, 1, response, NULL, 0, 0x7fffffffffffffffLL, &candidate);

        usdk_vote_t vote;
        usdk_perceive_vote(perceive, &candidate, &vote);
        printf("vote on fabricated evidence: %s (%s: %s)\n",
               vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT", vote.reason_code, vote.reason_detail);
    }

    usdk_perceive_destroy(perceive);
    return 0;
}
