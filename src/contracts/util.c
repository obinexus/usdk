#include "usdk/types.h"
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>

int64_t usdk_monotonic_ns(void) {
    static LARGE_INTEGER freq;
    static int have_freq = 0;
    if (!have_freq) { QueryPerformanceFrequency(&freq); have_freq = 1; }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    /* Split into whole-seconds and remainder to avoid overflowing
     * int64_t when multiplying a large counter by 1e9 before dividing. */
    long long whole = now.QuadPart / freq.QuadPart;
    long long rem = now.QuadPart % freq.QuadPart;
    return (int64_t)(whole * 1000000000LL + (rem * 1000000000LL) / freq.QuadPart);
}
#else
#include <time.h>

int64_t usdk_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}
#endif

void usdk_owned_buffer_release(usdk_owned_buffer_t* buf) {
    if (!buf) return;
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}
