/* A deliberately ABI-incompatible module: usdk-ffi must detect this from
 * the real usdk_plugin_query_v1 call and refuse it before ever touching
 * the vtable it returns - see docs/ABI.md "ABI version and struct-size
 * negotiation" and tests/integration/test_ffi_abi_mismatch.c. */

#include "usdk/plugin.h"
#include <stdlib.h>

static usdk_status_t bad_create(const usdk_config_t* cfg, usdk_role_instance_t** out) {
    (void)cfg; (void)out; return USDK_ERR_NOT_IMPLEMENTED;
}
static void bad_destroy(usdk_role_instance_t* inst) { (void)inst; }
static usdk_status_t bad_vote(usdk_role_instance_t* inst, const usdk_candidate_t* c, usdk_vote_t* v) {
    (void)inst; (void)c; (void)v; return USDK_ERR_NOT_IMPLEMENTED;
}

static const usdk_role_vtable_t g_vtable = {
    (uint32_t)sizeof(usdk_role_vtable_t), bad_create, bad_destroy, bad_vote, NULL
};

static const usdk_descriptor_t g_descriptor = {
    (uint32_t)sizeof(usdk_descriptor_t),
    USDK_ABI_VERSION_MAJOR + 1u, /* one past what this build supports */
    0,
    USDK_ROLE_PERCEIVE,
    "module-bad-abi",
    USDK_CAP_VOTE
};

USDK_EXPORT usdk_status_t USDK_CALL usdk_plugin_query_v1(
    const usdk_descriptor_t** out_descriptor, const void** out_vtable) {
    if (!out_descriptor || !out_vtable) return USDK_ERR_INVALID_ARGUMENT;
    *out_descriptor = &g_descriptor;
    *out_vtable = &g_vtable;
    return USDK_OK;
}
