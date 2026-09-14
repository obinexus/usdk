#include "usdk/deliberate.h"
#include "usdk/wire.h"
#include "usdk/ffi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../ffi/json_min.h"

#define USDK_DELIBERATE_HISTORY_CAP 64

struct usdk_role_instance {
    char party_id[USDK_ID_LEN];

    usdk_module_t*               driver_module;
    const usdk_driver_vtable_t*  driver_vt;
    usdk_role_instance_t*        driver_inst; /* the loaded driver's own
                                                * opaque instance - a
                                                * distinct type identity
                                                * from `this`, even though
                                                * both alias
                                                * usdk_role_instance_t;
                                                * never dereferenced here,
                                                * only passed back into
                                                * driver_vt. */

    uint8_t  generated_digests[USDK_DELIBERATE_HISTORY_CAP][USDK_DIGEST_LEN];
    uint32_t next_history_slot; /* ring buffer, oldest evicted first */
    uint32_t history_count;
};

usdk_status_t usdk_deliberate_create(const usdk_config_t* cfg, usdk_role_instance_t** out) {
    if (!out || !cfg || !cfg->data.data) return USDK_ERR_INVALID_ARGUMENT;

    char driver_path[USDK_ID_LEN * 4];
    driver_path[0] = '\0';
    char party_id[USDK_ID_LEN];
    strncpy(party_id, "usdk-deliberate", sizeof(party_id) - 1);
    party_id[sizeof(party_id) - 1] = '\0';

    const char* p = (const char*)cfg->data.data;
    const char* end = p + cfg->data.len;
    const char* v = usdk_json_find_key(p, end, "driver_path");
    if (!v || !usdk_json_parse_string(v, end, driver_path, sizeof(driver_path))) {
        return USDK_ERR_INVALID_ARGUMENT; /* driver_path is required - see usdk/deliberate.h */
    }
    v = usdk_json_find_key(p, end, "party_id");
    if (v) usdk_json_parse_string(v, end, party_id, sizeof(party_id));

    usdk_module_t* module = NULL;
    const usdk_descriptor_t* descriptor = NULL;
    const void* vtable = NULL;
    usdk_status_t st = usdk_ffi_load(driver_path, &module, &descriptor, &vtable);
    if (st != USDK_OK) return st;
    if (descriptor->role != USDK_ROLE_DRIVER || !(descriptor->capability_flags & USDK_CAP_DRIVER)) {
        usdk_ffi_unload(module);
        return USDK_ERR_CAPABILITY_NOT_FOUND;
    }

    const usdk_driver_vtable_t* driver_vt = (const usdk_driver_vtable_t*)vtable;
    if (driver_vt->struct_size < (uint32_t)sizeof(usdk_driver_vtable_t) || !driver_vt->create ||
        !driver_vt->destroy || !driver_vt->generate) {
        usdk_ffi_unload(module);
        return USDK_ERR_STRUCT_SIZE_MISMATCH;
    }

    usdk_role_instance_t* driver_inst = NULL;
    st = driver_vt->create(NULL, &driver_inst);
    if (st != USDK_OK) { usdk_ffi_unload(module); return st; }
    usdk_ffi_instance_created(module);

    usdk_role_instance_t* inst = (usdk_role_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) {
        driver_vt->destroy(driver_inst);
        usdk_ffi_instance_destroyed(module);
        usdk_ffi_unload(module);
        return USDK_ERR_OUT_OF_MEMORY;
    }
    strncpy(inst->party_id, party_id, USDK_ID_LEN - 1);
    inst->driver_module = module;
    inst->driver_vt = driver_vt;
    inst->driver_inst = driver_inst;

    *out = inst;
    return USDK_OK;
}

void usdk_deliberate_destroy(usdk_role_instance_t* inst) {
    if (!inst) return;
    if (inst->driver_vt && inst->driver_inst) inst->driver_vt->destroy(inst->driver_inst);
    if (inst->driver_module) {
        usdk_ffi_instance_destroyed(inst->driver_module);
        usdk_ffi_unload(inst->driver_module);
    }
    free(inst);
}

usdk_status_t usdk_deliberate_propose(
    usdk_role_instance_t* inst,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t prompt,
    usdk_owned_buffer_t* out_response) {
    if (!inst || !out_response) return USDK_ERR_INVALID_ARGUMENT;

    usdk_status_t st = inst->driver_vt->generate(inst->driver_inst, evidence_refs, evidence_ref_count,
                                                  prompt, out_response);
    if (st != USDK_OK) return st;

    uint8_t digest[USDK_DIGEST_LEN];
    usdk_sha256(out_response->data, out_response->len, digest);
    memcpy(inst->generated_digests[inst->next_history_slot % USDK_DELIBERATE_HISTORY_CAP], digest, USDK_DIGEST_LEN);
    inst->next_history_slot++;
    if (inst->history_count < USDK_DELIBERATE_HISTORY_CAP) inst->history_count++;

    return USDK_OK;
}

static void fill_vote(usdk_role_instance_t* inst, const usdk_candidate_t* candidate,
                       usdk_verdict_t verdict, const char* reason_code, const char* reason_detail,
                       usdk_vote_t* out_vote) {
    memset(out_vote, 0, sizeof(*out_vote));
    out_vote->struct_size = (uint32_t)sizeof(*out_vote);
    strncpy(out_vote->party_id, inst->party_id, USDK_ID_LEN - 1);
    out_vote->role = USDK_ROLE_DELIBERATE;
    out_vote->verdict = verdict;
    strncpy(out_vote->reason_code, reason_code, USDK_ID_LEN - 1);
    strncpy(out_vote->reason_detail, reason_detail, sizeof(out_vote->reason_detail) - 1);
    memcpy(out_vote->candidate_digest_seen, candidate->content_digest, USDK_DIGEST_LEN);
    out_vote->voted_at_ns = usdk_monotonic_ns();
}

usdk_status_t usdk_deliberate_vote(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote) {
    if (!inst || !candidate || !out_vote) return USDK_ERR_INVALID_ARGUMENT;

    uint8_t digest[USDK_DIGEST_LEN];
    usdk_sha256(candidate->proposed_response.data, candidate->proposed_response.len, digest);

    uint32_t to_check = inst->history_count;
    int found = 0;
    for (uint32_t i = 0; i < to_check; ++i) {
        if (memcmp(inst->generated_digests[i], digest, USDK_DIGEST_LEN) == 0) { found = 1; break; }
    }

    if (!found) {
        fill_vote(inst, candidate, USDK_VERDICT_REJECT, "not-self-generated",
                  "proposed_response does not match anything this instance produced via usdk_deliberate_propose",
                  out_vote);
        return USDK_OK;
    }

    fill_vote(inst, candidate, USDK_VERDICT_ACCEPT, "self-consistent",
              "proposed_response matches a response this instance generated", out_vote);
    return USDK_OK;
}

/* --- usdk_plugin_query_v1 --- */

static const usdk_role_vtable_t g_vtable = {
    (uint32_t)sizeof(usdk_role_vtable_t),
    usdk_deliberate_create,
    usdk_deliberate_destroy,
    usdk_deliberate_vote,
    usdk_deliberate_propose
};

static const usdk_descriptor_t g_descriptor = {
    (uint32_t)sizeof(usdk_descriptor_t),
    USDK_ABI_VERSION_MAJOR,
    USDK_ABI_VERSION_MINOR,
    USDK_ROLE_DELIBERATE,
    "usdk-deliberate",
    USDK_CAP_VOTE | USDK_CAP_PROPOSE
};

USDK_EXPORT usdk_status_t USDK_CALL usdk_plugin_query_v1(
    const usdk_descriptor_t** out_descriptor, const void** out_vtable) {
    if (!out_descriptor || !out_vtable) return USDK_ERR_INVALID_ARGUMENT;
    *out_descriptor = &g_descriptor;
    *out_vtable = &g_vtable;
    return USDK_OK;
}
