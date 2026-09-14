#ifndef USDK_JSON_MIN_H
#define USDK_JSON_MIN_H

#include <stddef.h>

/* A minimal, non-recursive JSON scanner for exactly the flat/one-level-
 * nested object shapes USDK's manifests and CLI config use - not a
 * general JSON library. Every function operates on a `[buf, buf+len)`
 * span and returns a pointer just past what it consumed, or NULL on
 * failure/not-found, so callers can chain calls without their own
 * bookkeeping. Internal only - not installed. */

const char* usdk_json_skip_ws(const char* p, const char* end);

/* Finds `"key"` followed by `:` anywhere in [p,end) at the current
 * object nesting level (does not descend into nested {}/[] while
 * searching), and returns a pointer to the start of its value (after
 * whitespace), or NULL if not found before `end`. */
const char* usdk_json_find_key(const char* p, const char* end, const char* key);

/* Parses a JSON string value starting at `p` (which must point at the
 * opening '"'). Copies the unescaped content (only \" \\ \/ \n \t
 * recognized - sufficient for USDK's own manifest content) into
 * `out`/`out_cap` (always NUL-terminated on success), and returns a
 * pointer just past the closing '"', or NULL on malformed input or if
 * the unescaped content does not fit `out_cap`. */
const char* usdk_json_parse_string(const char* p, const char* end, char* out, size_t out_cap);

/* Parses an unsigned decimal integer starting at `p`. */
const char* usdk_json_parse_uint(const char* p, const char* end, unsigned* out);

/* Parses a JSON boolean literal (`true`/`false`) starting at `p`. */
const char* usdk_json_parse_bool(const char* p, const char* end, int* out);

/* Returns a pointer just past the JSON value (string/number/bool/null/
 * object/array) starting at `p`, without extracting it - used to skip a
 * field this parser does not care about. */
const char* usdk_json_skip_value(const char* p, const char* end);

#endif /* USDK_JSON_MIN_H */
