#include "usdk/perceive.h"
#include "usdk/wire.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* See src/ffi/json_min.h - reused here as a source file, not a shared
 * library dependency, to keep usdk_perceive linking only Usdk::contracts
 * (docs/PACKAGES.md). */
#include "../ffi/json_min.h"

#define USDK_PERCEIVE_MAX_OBSERVED 256

typedef struct observed_evidence {
    char    evidence_id[USDK_ID_LEN];
    uint8_t digest[USDK_DIGEST_LEN];
    double  uncertainty;
    int     used;
} observed_evidence_t;

struct usdk_role_instance {
    char   party_id[USDK_ID_LEN];
    int    require_evidence;
    observed_evidence_t observed[USDK_PERCEIVE_MAX_OBSERVED];
    uint32_t next_slot; /* ring buffer - oldest evicted first once full */
    uint64_t next_evidence_seq;
};

usdk_status_t usdk_perceive_create(const usdk_config_t* cfg, usdk_role_instance_t** out) {
    if (!out) return USDK_ERR_INVALID_ARGUMENT;
    usdk_role_instance_t* inst = (usdk_role_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) return USDK_ERR_OUT_OF_MEMORY;
    strncpy(inst->party_id, "usdk-perceive", USDK_ID_LEN - 1);
    inst->require_evidence = 1;

    if (cfg && cfg->data.data && cfg->data.len > 0) {
        const char* p = (const char*)cfg->data.data;
        const char* end = p + cfg->data.len;
        const char* v = usdk_json_find_key(p, end, "require_evidence");
        if (v) { int b; if (usdk_json_parse_bool(v, end, &b)) inst->require_evidence = b; }
        v = usdk_json_find_key(p, end, "party_id");
        if (v) usdk_json_parse_string(v, end, inst->party_id, sizeof(inst->party_id));
    }

    *out = inst;
    return USDK_OK;
}

void usdk_perceive_destroy(usdk_role_instance_t* inst) { free(inst); }

usdk_status_t usdk_perceive_observe(
    usdk_role_instance_t* inst,
    usdk_buffer_t raw_input,
    double uncertainty,
    usdk_evidence_ref_t* out_ref) {
    if (!inst || !out_ref) return USDK_ERR_INVALID_ARGUMENT;
    if (uncertainty < 0.0) uncertainty = 0.0;
    if (uncertainty > 1.0) uncertainty = 1.0;

    observed_evidence_t* slot = &inst->observed[inst->next_slot % USDK_PERCEIVE_MAX_OBSERVED];
    snprintf(slot->evidence_id, sizeof(slot->evidence_id), "ev-%llu",
             (unsigned long long)inst->next_evidence_seq);
    usdk_sha256(raw_input.data, raw_input.len, slot->digest);
    slot->uncertainty = uncertainty;
    slot->used = 1;
    inst->next_slot++;
    inst->next_evidence_seq++;

    memset(out_ref, 0, sizeof(*out_ref));
    strncpy(out_ref->evidence_id, slot->evidence_id, USDK_ID_LEN - 1);
    memcpy(out_ref->evidence_digest, slot->digest, USDK_DIGEST_LEN);
    out_ref->uncertainty = uncertainty;
    return USDK_OK;
}

static void fill_vote(usdk_role_instance_t* inst, const usdk_candidate_t* candidate,
                       usdk_verdict_t verdict, const char* reason_code, const char* reason_detail,
                       usdk_vote_t* out_vote) {
    memset(out_vote, 0, sizeof(*out_vote));
    out_vote->struct_size = (uint32_t)sizeof(*out_vote);
    strncpy(out_vote->party_id, inst->party_id, USDK_ID_LEN - 1);
    out_vote->role = USDK_ROLE_PERCEIVE;
    out_vote->verdict = verdict;
    strncpy(out_vote->reason_code, reason_code, USDK_ID_LEN - 1);
    strncpy(out_vote->reason_detail, reason_detail, sizeof(out_vote->reason_detail) - 1);
    memcpy(out_vote->candidate_digest_seen, candidate->content_digest, USDK_DIGEST_LEN);
    out_vote->voted_at_ns = usdk_monotonic_ns();
}

usdk_status_t usdk_perceive_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote) {
    if (!inst || !candidate || !out_vote) return USDK_ERR_INVALID_ARGUMENT;

    if (inst->require_evidence && candidate->evidence_ref_count == 0) {
        fill_vote(inst, candidate, USDK_VERDICT_REJECT, "no-evidence-cited",
                  "candidate cites zero evidence references", out_vote);
        return USDK_OK;
    }

    for (uint32_t i = 0; i < candidate->evidence_ref_count; ++i) {
        const usdk_evidence_ref_t* ref = &candidate->evidence_refs[i];
        int found = 0;
        for (uint32_t s = 0; s < USDK_PERCEIVE_MAX_OBSERVED; ++s) {
            observed_evidence_t* obs = &inst->observed[s];
            if (!obs->used) continue;
            if (strncmp(obs->evidence_id, ref->evidence_id, USDK_ID_LEN) == 0 &&
                memcmp(obs->digest, ref->evidence_digest, USDK_DIGEST_LEN) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char detail[256];
            snprintf(detail, sizeof(detail), "evidence_ref '%s' does not match any observation this instance recorded",
                      ref->evidence_id);
            fill_vote(inst, candidate, USDK_VERDICT_REJECT, "evidence-not-found", detail, out_vote);
            return USDK_OK;
        }
    }

    fill_vote(inst, candidate, USDK_VERDICT_ACCEPT, "evidence-verified",
              "every cited evidence_ref matches an observation this instance recorded", out_vote);
    return USDK_OK;
}

/* --- usdk_plugin_query_v1: makes this same implementation discoverable
 * and loadable dynamically, in addition to the direct-link functions
 * above - see docs/ABI.md "Entry point" and usdk/perceive.h. */

static const usdk_role_vtable_t g_vtable = {
    (uint32_t)sizeof(usdk_role_vtable_t),
    usdk_perceive_create,
    usdk_perceive_destroy,
    usdk_perceive_vote,
    NULL /* propose - not applicable to usdk-perceive */
};

static const usdk_descriptor_t g_descriptor = {
    (uint32_t)sizeof(usdk_descriptor_t),
    USDK_ABI_VERSION_MAJOR,
    USDK_ABI_VERSION_MINOR,
    USDK_ROLE_PERCEIVE,
    "usdk-perceive",
    USDK_CAP_VOTE
};

USDK_EXPORT usdk_status_t USDK_CALL usdk_plugin_query_v1(
    const usdk_descriptor_t** out_descriptor, const void** out_vtable) {
    if (!out_descriptor || !out_vtable) return USDK_ERR_INVALID_ARGUMENT;
    *out_descriptor = &g_descriptor;
    *out_vtable = &g_vtable;
    return USDK_OK;
}
