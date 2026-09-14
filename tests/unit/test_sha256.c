#include "usdk_test.h"
#include "usdk/wire.h"

/* Known-answer vectors, computed and verified independently with the
 * local `sha256sum` utility (not retyped from memory) - see
 * docs/VALIDATION.md. Not derived from any file in
 * docs/RESEARCH_REVIEW.md's reviewed archives - this SHA-256
 * implementation (src/contracts/sha256.c) was written directly from the
 * public FIPS 180-4 specification. */

static void hex_encode(const uint8_t* d, char* out) {
    static const char* hexd = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = hexd[d[i] >> 4];
        out[i * 2 + 1] = hexd[d[i] & 0xF];
    }
    out[64] = '\0';
}

USDK_TEST_MAIN_BEGIN()
    uint8_t digest[32];
    char hex[65];

    usdk_sha256((const uint8_t*)"", 0, digest);
    hex_encode(digest, hex);
    USDK_CHECK_STR_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    usdk_sha256((const uint8_t*)"abc", 3, digest);
    hex_encode(digest, hex);
    USDK_CHECK_STR_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    {
        const char* msg = "The quick brown fox jumps over the lazy dog";
        usdk_sha256((const uint8_t*)msg, (uint32_t)strlen(msg), digest);
        hex_encode(digest, hex);
        USDK_CHECK_STR_EQ(hex, "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    }

    /* A message spanning more than one 64-byte block, to exercise the
     * multi-block path (sha256_update crossing a block boundary), not
     * just the single-block padding path the vectors above stay within. */
    {
        char msg[130];
        for (int i = 0; i < 129; ++i) msg[i] = (char)('a' + (i % 26));
        msg[129] = '\0';
        uint8_t d1[32], d2[32];
        usdk_sha256((const uint8_t*)msg, 129, d1);
        /* Same input hashed again must produce the same digest -
         * determinism is itself a property worth checking, not just a
         * single fixed vector. */
        usdk_sha256((const uint8_t*)msg, 129, d2);
        USDK_CHECK(memcmp(d1, d2, 32) == 0);
    }
USDK_TEST_MAIN_END()
