#include "usdk/candidate.h"
#include "usdk/wire.h"
#include <string.h>
#include <stdlib.h>

static int id_fits(const char* s) {
    if (!s) return 0;
    return strnlen(s, USDK_ID_LEN) < USDK_ID_LEN; /* must have a NUL within USDK_ID_LEN bytes */
}

usdk_status_t usdk_candidate_create(
    uint32_t policy_version,
    const char* session_id,
    uint64_t round_id,
    const char* candidate_id,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t proposed_response,
    const usdk_constraint_t* constraints, uint32_t constraint_count,
    int64_t deadline_ns,
    usdk_candidate_t* out) {
    if (!out || !id_fits(session_id) || !id_fits(candidate_id)) return USDK_ERR_INVALID_ARGUMENT;
    if (evidence_ref_count > 0 && !evidence_refs) return USDK_ERR_INVALID_ARGUMENT;
    if (constraint_count > 0 && !constraints) return USDK_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    out->struct_size = (uint32_t)sizeof(*out);
    out->protocol_version = USDK_PROTOCOL_VERSION_V1;
    out->policy_version = policy_version;
    strncpy(out->session_id, session_id, USDK_ID_LEN - 1);
    out->round_id = round_id;
    strncpy(out->candidate_id, candidate_id, USDK_ID_LEN - 1);
    out->evidence_refs = evidence_refs;
    out->evidence_ref_count = evidence_ref_count;
    out->proposed_response = proposed_response;
    out->constraints = constraints;
    out->constraint_count = constraint_count;
    out->deadline_ns = deadline_ns;

    uint32_t needed = usdk_wire_candidate_encoded_size(out);
    uint8_t stack_buf[1024];
    uint8_t* buf = stack_buf;
    uint8_t* heap_buf = NULL;
    if (needed > sizeof(stack_buf)) {
        heap_buf = (uint8_t*)malloc(needed);
        if (!heap_buf) return USDK_ERR_OUT_OF_MEMORY;
        buf = heap_buf;
    }
    uint32_t written = usdk_wire_encode_candidate(out, buf, needed > sizeof(stack_buf) ? needed : (uint32_t)sizeof(stack_buf));
    if (written == 0) { free(heap_buf); return USDK_ERR_INVALID_ARGUMENT; }
    usdk_sha256(buf, written, out->content_digest);
    free(heap_buf);
    return USDK_OK;
}

int usdk_candidate_digest_matches(const usdk_candidate_t* c) {
    if (!c) return 0;
    uint32_t needed = usdk_wire_candidate_encoded_size(c);
    uint8_t stack_buf[1024];
    uint8_t* buf = stack_buf;
    uint8_t* heap_buf = NULL;
    if (needed > sizeof(stack_buf)) {
        heap_buf = (uint8_t*)malloc(needed);
        if (!heap_buf) return 0;
        buf = heap_buf;
    }
    uint32_t written = usdk_wire_encode_candidate(c, buf, needed > sizeof(stack_buf) ? needed : (uint32_t)sizeof(stack_buf));
    if (written == 0) { free(heap_buf); return 0; }
    uint8_t digest[USDK_DIGEST_LEN];
    usdk_sha256(buf, written, digest);
    int match = memcmp(digest, c->content_digest, USDK_DIGEST_LEN) == 0;
    free(heap_buf);
    return match;
}
