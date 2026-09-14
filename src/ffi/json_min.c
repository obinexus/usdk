#include "json_min.h"
#include <string.h>
#include <ctype.h>

const char* usdk_json_skip_ws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    return p;
}

const char* usdk_json_skip_value(const char* p, const char* end) {
    p = usdk_json_skip_ws(p, end);
    if (p >= end) return NULL;
    if (*p == '"') {
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) p += 2; else ++p;
        }
        return (p < end) ? p + 1 : NULL;
    }
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 1;
        ++p;
        while (p < end && depth > 0) {
            if (*p == '"') {
                ++p;
                while (p < end && *p != '"') { if (*p == '\\' && p + 1 < end) p += 2; else ++p; }
                if (p < end) ++p;
                continue;
            }
            if (*p == open) ++depth;
            else if (*p == close) --depth;
            ++p;
        }
        return (depth == 0) ? p : NULL;
    }
    /* number, true, false, null */
    while (p < end && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        ++p;
    }
    return p;
}

const char* usdk_json_find_key(const char* p, const char* end, const char* key) {
    size_t key_len = strlen(key);
    const char* cur = p;
    int depth = 0;
    while (cur < end) {
        if (*cur == '"') {
            const char* str_start = cur + 1;
            const char* q = str_start;
            while (q < end && *q != '"') { if (*q == '\\' && q + 1 < end) q += 2; else ++q; }
            if (q >= end) return NULL;
            size_t str_len = (size_t)(q - str_start);
            const char* after_quote = q + 1;
            const char* colon = usdk_json_skip_ws(after_quote, end);
            if (depth == 1 && colon < end && *colon == ':' &&
                str_len == key_len && memcmp(str_start, key, key_len) == 0) {
                return usdk_json_skip_ws(colon + 1, end);
            }
            cur = after_quote;
            continue;
        }
        if (*cur == '{' || *cur == '[') { ++depth; ++cur; continue; }
        if (*cur == '}' || *cur == ']') { --depth; ++cur; if (depth <= 0) return NULL; continue; }
        ++cur;
    }
    return NULL;
}

const char* usdk_json_parse_string(const char* p, const char* end, char* out, size_t out_cap) {
    if (p >= end || *p != '"') return NULL;
    ++p;
    size_t oi = 0;
    while (p < end && *p != '"') {
        char c = *p;
        if (c == '\\' && p + 1 < end) {
            char e = p[1];
            switch (e) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                default: c = e; break;
            }
            p += 2;
        } else {
            ++p;
        }
        if (oi + 1 >= out_cap) return NULL;
        out[oi++] = c;
    }
    if (p >= end) return NULL;
    out[oi] = '\0';
    return p + 1;
}

const char* usdk_json_parse_uint(const char* p, const char* end, unsigned* out) {
    p = usdk_json_skip_ws(p, end);
    if (p >= end || !isdigit((unsigned char)*p)) return NULL;
    unsigned v = 0;
    while (p < end && isdigit((unsigned char)*p)) {
        v = v * 10u + (unsigned)(*p - '0');
        ++p;
    }
    *out = v;
    return p;
}

const char* usdk_json_parse_bool(const char* p, const char* end, int* out) {
    p = usdk_json_skip_ws(p, end);
    if (end - p >= 4 && memcmp(p, "true", 4) == 0) { *out = 1; return p + 4; }
    if (end - p >= 5 && memcmp(p, "false", 5) == 0) { *out = 0; return p + 5; }
    return NULL;
}
