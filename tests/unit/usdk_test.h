#ifndef USDK_TEST_H
#define USDK_TEST_H

/* Header-only minimal test-check macros shared by every unit test file
 * under tests/unit/ - not part of the public ABI, not installed. Each
 * test file's
 * main() returns the failure count (0 = pass) so CTest sees a real exit
 * code, and every failure prints file:line plus both sides of the
 * comparison so a failure is diagnosable from CI output alone. */

#include <stdio.h>
#include <string.h>

static int g_usdk_test_failures = 0;

#define USDK_CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_usdk_test_failures++; \
        } \
    } while (0)

#define USDK_CHECK_EQ_INT(a, b) \
    do { \
        long long _a = (long long)(a), _b = (long long)(b); \
        if (_a != _b) { \
            fprintf(stderr, "FAIL %s:%d: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #a, _a, #b, _b); \
            g_usdk_test_failures++; \
        } \
    } while (0)

#define USDK_CHECK_STR_EQ(a, b) \
    do { \
        const char* _a = (a); const char* _b = (b); \
        if (strcmp(_a, _b) != 0) { \
            fprintf(stderr, "FAIL %s:%d: %s (\"%s\") != %s (\"%s\")\n", __FILE__, __LINE__, #a, _a, #b, _b); \
            g_usdk_test_failures++; \
        } \
    } while (0)

#define USDK_TEST_MAIN_BEGIN() int main(void) {
#define USDK_TEST_MAIN_END() \
    if (g_usdk_test_failures == 0) fprintf(stderr, "PASS\n"); \
    else fprintf(stderr, "FAIL (%d)\n", g_usdk_test_failures); \
    return g_usdk_test_failures == 0 ? 0 : 1; \
    }

#endif /* USDK_TEST_H */
