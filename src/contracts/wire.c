#include "usdk/wire.h"
#include <string.h>

/* All multi-byte integers are written little-endian; every id field is
 * written as exactly USDK_ID_LEN bytes (the source string plus NUL
 * padding, truncated - never overrun - if the source somehow lacks a
 * terminator within that span); see usdk/wire.h.
 *
 * Every put_* helper takes an explicit `ok` flag rather than signaling
 * overflow by returning 0: offset 0 is a legitimate position (the very
 * first field), so overloading it as an error sentinel would let a
 * failed write silently be mistaken for "start over from the beginning"
 * by the next call in the sequence, corrupting already-written bytes
 * instead of failing loudly. Once `*ok` is false, every subsequent
 * put_* is a no-op that returns `pos` unchanged. */

static uint32_t put_u32(uint8_t* out, uint32_t cap, uint32_t pos, uint32_t v, int* ok) {
    if (!*ok) return pos;
    if (pos + 4 > cap) { *ok = 0; return pos; }
    out[pos] = (uint8_t)(v); out[pos+1] = (uint8_t)(v>>8);
    out[pos+2] = (uint8_t)(v>>16); out[pos+3] = (uint8_t)(v>>24);
    return pos + 4;
}
static uint32_t put_u64(uint8_t* out, uint32_t cap, uint32_t pos, uint64_t v, int* ok) {
    if (!*ok) return pos;
    if (pos + 8 > cap) { *ok = 0; return pos; }
    for (int i = 0; i < 8; ++i) out[pos + i] = (uint8_t)(v >> (8 * i));
    return pos + 8;
}
static uint32_t put_i64(uint8_t* out, uint32_t cap, uint32_t pos, int64_t v, int* ok) {
    return put_u64(out, cap, pos, (uint64_t)v, ok);
}
static uint32_t put_id(uint8_t* out, uint32_t cap, uint32_t pos, const char* id, int* ok) {
    if (!*ok) return pos;
    if (pos + USDK_ID_LEN > cap) { *ok = 0; return pos; }
    memset(out + pos, 0, USDK_ID_LEN);
    size_t n = strnlen(id, USDK_ID_LEN - 1);
    memcpy(out + pos, id, n);
    return pos + USDK_ID_LEN;
}
static uint32_t put_bytes(uint8_t* out, uint32_t cap, uint32_t pos, const uint8_t* data, uint32_t len, int* ok) {
    if (!*ok) return pos;
    if (pos + len > cap) { *ok = 0; return pos; }
    if (len) memcpy(out + pos, data, len);
    return pos + len;
}

uint32_t usdk_wire_candidate_encoded_size(const usdk_candidate_t* c) {
    if (!c) return 0;
    uint32_t n = 0;
    n += 4 /* protocol_version */ + 4 /* policy_version */;
    n += USDK_ID_LEN /* session_id */;
    n += 8 /* round_id */;
    n += USDK_ID_LEN /* candidate_id */;
    n += 4 /* evidence_ref_count */;
    n += c->evidence_ref_count * (USDK_ID_LEN + USDK_DIGEST_LEN + 8 /* uncertainty as 8 bytes */);
    n += 4 /* proposed_response length */ + c->proposed_response.len;
    n += 4 /* constraint_count */;
    n += c->constraint_count * USDK_ID_LEN;
    n += 8 /* deadline_ns */;
    return n;
}

uint32_t usdk_wire_encode_candidate(const usdk_candidate_t* c, uint8_t* out, uint32_t out_cap) {
    if (!c || !out) return 0;
    uint32_t needed = usdk_wire_candidate_encoded_size(c);
    if (needed == 0 || needed > out_cap) return 0;

    int ok = 1;
    uint32_t pos = 0;
    pos = put_u32(out, out_cap, pos, c->protocol_version, &ok);
    pos = put_u32(out, out_cap, pos, c->policy_version, &ok);
    pos = put_id(out, out_cap, pos, c->session_id, &ok);
    pos = put_u64(out, out_cap, pos, c->round_id, &ok);
    pos = put_id(out, out_cap, pos, c->candidate_id, &ok);

    pos = put_u32(out, out_cap, pos, c->evidence_ref_count, &ok);
    for (uint32_t i = 0; i < c->evidence_ref_count && ok; ++i) {
        const usdk_evidence_ref_t* r = &c->evidence_refs[i];
        pos = put_id(out, out_cap, pos, r->evidence_id, &ok);
        pos = put_bytes(out, out_cap, pos, r->evidence_digest, USDK_DIGEST_LEN, &ok);
        uint64_t bits;
        memcpy(&bits, &r->uncertainty, sizeof(bits));
        pos = put_u64(out, out_cap, pos, bits, &ok);
    }

    pos = put_u32(out, out_cap, pos, c->proposed_response.len, &ok);
    pos = put_bytes(out, out_cap, pos, c->proposed_response.data, c->proposed_response.len, &ok);

    pos = put_u32(out, out_cap, pos, c->constraint_count, &ok);
    for (uint32_t i = 0; i < c->constraint_count && ok; ++i) {
        pos = put_id(out, out_cap, pos, c->constraints[i].constraint_id, &ok);
    }

    pos = put_i64(out, out_cap, pos, c->deadline_ns, &ok);

    if (!ok) return 0; /* unreachable given the needed<=out_cap precheck
                         * above, kept as defense against the encoding
                         * sequence and usdk_wire_candidate_encoded_size
                         * ever drifting apart. */
    return pos;
}
