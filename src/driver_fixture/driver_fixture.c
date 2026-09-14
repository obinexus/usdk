#include "usdk/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* usdk-driver-fixture: a deterministic, offline candidate-generation
 * driver for usdk-deliberate, used by every test and demo in this
 * repository that needs reproducible output - see
 * docs/IMPLEMENTATION_STATUS.md "Vertical slice". Every response this
 * driver produces is prefixed with a literal "[usdk-driver-fixture]"
 * marker so it can never be mistaken for real model inference - per the
 * task brief: "Do not present fixture responses as real model
 * inference." There is no other driver in this release (see
 * docs/IMPLEMENTATION_STATUS.md for why a real local-inference driver
 * was not added). */

struct usdk_role_instance {
    int placeholder; /* this driver is stateless; the instance exists
                       * only to give usdk-deliberate a handle to hold,
                       * matching the ABI's lifecycle contract. */
};

static usdk_status_t fixture_create(const usdk_config_t* cfg, usdk_role_instance_t** out) {
    (void)cfg;
    if (!out) return USDK_ERR_INVALID_ARGUMENT;
    usdk_role_instance_t* inst = (usdk_role_instance_t*)calloc(1, sizeof(*inst));
    if (!inst) return USDK_ERR_OUT_OF_MEMORY;
    *out = inst;
    return USDK_OK;
}

static void fixture_destroy(usdk_role_instance_t* inst) { free(inst); }

static usdk_status_t fixture_generate(
    usdk_role_instance_t* inst,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t prompt,
    usdk_owned_buffer_t* out_response) {
    (void)inst;
    if (!out_response) return USDK_ERR_INVALID_ARGUMENT;

    /* Deterministic: the same prompt + evidence always produces the same
     * bytes, on purpose - this is what makes tests built on this driver
     * reproducible, unlike a real inference backend. */
    double max_uncertainty = 0.0;
    for (uint32_t i = 0; i < evidence_ref_count; ++i) {
        if (evidence_refs[i].uncertainty > max_uncertainty) max_uncertainty = evidence_refs[i].uncertainty;
    }

    uint32_t cap = prompt.len + 256;
    char* buf = (char*)malloc(cap);
    if (!buf) return USDK_ERR_OUT_OF_MEMORY;
    int n = snprintf(buf, cap,
        "[usdk-driver-fixture] deterministic fixture response - not real model inference. "
        "evidence_count=%u max_uncertainty=%.4f prompt=\"%.*s\"",
        evidence_ref_count, max_uncertainty, (int)prompt.len, (const char*)prompt.data);
    if (n < 0) { free(buf); return USDK_ERR_INVALID_ARGUMENT; }

    out_response->data = (uint8_t*)buf;
    out_response->len = (uint32_t)n;
    out_response->cap = cap;
    return USDK_OK;
}

static const usdk_driver_vtable_t g_vtable = {
    (uint32_t)sizeof(usdk_driver_vtable_t),
    fixture_create,
    fixture_destroy,
    fixture_generate
};

static const usdk_descriptor_t g_descriptor = {
    (uint32_t)sizeof(usdk_descriptor_t),
    USDK_ABI_VERSION_MAJOR,
    USDK_ABI_VERSION_MINOR,
    USDK_ROLE_DRIVER,
    "usdk-driver-fixture",
    USDK_CAP_DRIVER
};

USDK_EXPORT usdk_status_t USDK_CALL usdk_plugin_query_v1(
    const usdk_descriptor_t** out_descriptor, const void** out_vtable) {
    if (!out_descriptor || !out_vtable) return USDK_ERR_INVALID_ARGUMENT;
    *out_descriptor = &g_descriptor;
    *out_vtable = &g_vtable;
    return USDK_OK;
}
