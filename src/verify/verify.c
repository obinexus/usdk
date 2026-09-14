#include "usdk/verify.h"
#include "usdk/wire.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../ffi/json_min.h"

#define USDK_VERIFY_MAX_ALLOWED_CONSTRAINTS 32

struct usdk_role_instance {
    char     party_id[USDK_ID_LEN];
    char     allowed_constraints[USDK_VERIFY_MAX_ALLOWED_CONSTRAINTS][USDK_ID_LEN];
    uint32_t allowed_constraint_count;
    double   max_uncertainty;
};

usdk_status_t usdk_verify_create(const usdk_config_t* cfg, usdk_role_instance_t** out) {
    if (!out) return USDK_ERR_INVALID_ARGUMENT;
    usdk_role_instance_t* inst = (usdk_role_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) return USDK_ERR_OUT_OF_MEMORY;
    strncpy(inst->party_id, "usdk-verify", USDK_ID_LEN - 1);
    inst->max_uncertainty = 0.5; /* caller-overridable policy default, not
                                  * a derived/proven constant - see
                                  * usdk/verify.h and
                                  * docs/RESEARCH_REVIEW.md section
                                  * 1.3.5 on why no threshold here is
                                  * claimed to be principled. */

    if (cfg && cfg->data.data && cfg->data.len > 0) {
        const char* p = (const char*)cfg->data.data;
        const char* end = p + cfg->data.len;

        const char* v = usdk_json_find_key(p, end, "party_id");
        if (v) usdk_json_parse_string(v, end, inst->party_id, sizeof(inst->party_id));

        v = usdk_json_find_key(p, end, "max_uncertainty");
        if (v) {
            /* usdk_json_parse_uint only handles integers; a fractional
             * max_uncertainty is read with strtod directly against the
             * same span instead of extending the minimal JSON scanner
             * for one float field. */
            char* endp = NULL;
            double d = strtod(v, &endp);
            if (endp != v) inst->max_uncertainty = d;
        }

        v = usdk_json_find_key(p, end, "allowed_constraints");
        if (v && v < end && *v == '[') {
            const char* cur = v + 1;
            for (;;) {
                cur = usdk_json_skip_ws(cur, end);
                if (cur >= end || *cur == ']') break;
                if (*cur == ',') { ++cur; continue; }
                if (inst->allowed_constraint_count >= USDK_VERIFY_MAX_ALLOWED_CONSTRAINTS) break;
                const char* next = usdk_json_parse_string(cur, end,
                    inst->allowed_constraints[inst->allowed_constraint_count], USDK_ID_LEN);
                if (!next) break;
                inst->allowed_constraint_count++;
                cur = next;
            }
        }
    }

    *out = inst;
    return USDK_OK;
}

void usdk_verify_destroy(usdk_role_instance_t* inst) { free(inst); }

static int constraint_allowed(usdk_role_instance_t* inst, const char* id) {
    for (uint32_t i = 0; i < inst->allowed_constraint_count; ++i) {
        if (strncmp(inst->allowed_constraints[i], id, USDK_ID_LEN) == 0) return 1;
    }
    return 0;
}

static void fill_vote(usdk_role_instance_t* inst, const usdk_candidate_t* candidate,
                       usdk_verdict_t verdict, const char* reason_code, const char* reason_detail,
                       usdk_vote_t* out_vote) {
    memset(out_vote, 0, sizeof(*out_vote));
    out_vote->struct_size = (uint32_t)sizeof(*out_vote);
    strncpy(out_vote->party_id, inst->party_id, USDK_ID_LEN - 1);
    out_vote->role = USDK_ROLE_VERIFY;
    out_vote->verdict = verdict;
    strncpy(out_vote->reason_code, reason_code, USDK_ID_LEN - 1);
    strncpy(out_vote->reason_detail, reason_detail, sizeof(out_vote->reason_detail) - 1);
    memcpy(out_vote->candidate_digest_seen, candidate->content_digest, USDK_DIGEST_LEN);
    out_vote->voted_at_ns = usdk_monotonic_ns();
}

usdk_status_t usdk_verify_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote) {
    if (!inst || !candidate || !out_vote) return USDK_ERR_INVALID_ARGUMENT;

    for (uint32_t i = 0; i < candidate->constraint_count; ++i) {
        if (!constraint_allowed(inst, candidate->constraints[i].constraint_id)) {
            char detail[256];
            snprintf(detail, sizeof(detail), "constraint '%s' is not in this instance's allowed_constraints policy",
                      candidate->constraints[i].constraint_id);
            fill_vote(inst, candidate, USDK_VERDICT_REJECT, "constraint-not-permitted", detail, out_vote);
            return USDK_OK;
        }
    }

    if (candidate->evidence_ref_count == 0) {
        fill_vote(inst, candidate, USDK_VERDICT_REJECT, "insufficient-evidence",
                  "candidate cites zero evidence references", out_vote);
        return USDK_OK;
    }

    for (uint32_t i = 0; i < candidate->evidence_ref_count; ++i) {
        if (candidate->evidence_refs[i].uncertainty > inst->max_uncertainty) {
            char detail[256];
            snprintf(detail, sizeof(detail),
                      "evidence_ref '%s' uncertainty %.4f exceeds this instance's policy max_uncertainty %.4f",
                      candidate->evidence_refs[i].evidence_id, candidate->evidence_refs[i].uncertainty,
                      inst->max_uncertainty);
            fill_vote(inst, candidate, USDK_VERDICT_REJECT, "uncertainty-exceeds-policy", detail, out_vote);
            return USDK_OK;
        }
    }

    fill_vote(inst, candidate, USDK_VERDICT_ACCEPT, "constraints-and-evidence-sufficient",
              "every constraint is permitted and every evidence_ref is within the uncertainty policy", out_vote);
    return USDK_OK;
}

/* --- usdk_plugin_query_v1 --- */

static const usdk_role_vtable_t g_vtable = {
    (uint32_t)sizeof(usdk_role_vtable_t),
    usdk_verify_create,
    usdk_verify_destroy,
    usdk_verify_vote,
    NULL /* propose - not applicable to usdk-verify */
};

static const usdk_descriptor_t g_descriptor = {
    (uint32_t)sizeof(usdk_descriptor_t),
    USDK_ABI_VERSION_MAJOR,
    USDK_ABI_VERSION_MINOR,
    USDK_ROLE_VERIFY,
    "usdk-verify",
    USDK_CAP_VOTE
};

USDK_EXPORT usdk_status_t USDK_CALL usdk_plugin_query_v1(
    const usdk_descriptor_t** out_descriptor, const void** out_vtable) {
    if (!out_descriptor || !out_vtable) return USDK_ERR_INVALID_ARGUMENT;
    *out_descriptor = &g_descriptor;
    *out_vtable = &g_vtable;
    return USDK_OK;
}
