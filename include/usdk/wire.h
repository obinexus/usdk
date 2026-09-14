#ifndef USDK_WIRE_H
#define USDK_WIRE_H

#include "usdk/types.h"
#include "usdk/status.h"
#include "usdk/candidate.h"

USDK_BEGIN_DECLS

/* SHA-256 over `data[0..len)`. Not a cryptographic-identity mechanism by
 * itself - see docs/CONSENSUS_PROTOCOL.md "Digest is not
 * authentication". */
USDK_EXPORT void USDK_CALL usdk_sha256(const uint8_t* data, uint32_t len,
                                        uint8_t out_digest[USDK_DIGEST_LEN]);

/* Encodes every field of `c` except struct_size and content_digest into
 * `out` as fixed-width, explicitly-ordered bytes (uint32/uint64/int64
 * written little-endian; every char[] id field written as exactly
 * USDK_ID_LEN bytes, NUL-padded; evidence_refs/constraints written as a
 * count followed by that many fixed-size records; proposed_response
 * written as a uint32 length followed by that many bytes) - never raw
 * pointers, never compiler struct padding. This is the "serialized
 * message" form referenced throughout docs/ABI.md, kept deliberately
 * separate from the in-process usdk_candidate_t layout.
 *
 * Returns the number of bytes written, or 0 if `out_cap` is too small
 * (the caller should retry with a larger buffer sized from a prior
 * usdk_wire_candidate_encoded_size call - this function never partially
 * writes). */
USDK_EXPORT uint32_t USDK_CALL usdk_wire_encode_candidate(
    const usdk_candidate_t* c, uint8_t* out, uint32_t out_cap);

/* Exact size usdk_wire_encode_candidate will need for `c`. */
USDK_EXPORT uint32_t USDK_CALL usdk_wire_candidate_encoded_size(const usdk_candidate_t* c);

USDK_END_DECLS

#endif /* USDK_WIRE_H */
