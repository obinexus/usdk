/* Standalone example: usdk-deliberate used entirely on its own, linked
 * directly - no usdk-perceive, no usdk-verify. Demonstrates proposing a
 * response via the fixture driver (loaded dynamically through
 * usdk-ffi, which usdk-deliberate depends on internally - see
 * docs/PACKAGES.md), then usdk_deliberate_vote both accepting a
 * self-generated candidate and rejecting one it never produced - the
 * "distinct check" this role performs (docs/ARCHITECTURE.md). */

#include <stdio.h>
#include <string.h>
#include "usdk/deliberate.h"
#include "usdk/candidate.h"

#if defined(_WIN32)
#include <windows.h>
#define USDK_PLATFORM_EXT ".dll"
#else
#include <unistd.h>
#if defined(__APPLE__)
#define USDK_PLATFORM_EXT ".dylib"
#else
#define USDK_PLATFORM_EXT ".so"
#endif
#endif

static void own_exe_dir(char* out, size_t out_cap) {
#if defined(_WIN32)
    char path[1024];
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    if (n > 0 && n < sizeof(path)) {
        char* slash = strrchr(path, '\\');
        if (slash) { size_t len = (size_t)(slash - path); if (len < out_cap) { memcpy(out, path, len); out[len] = '\0'; return; } }
    }
#else
    char path[1024];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0) {
        path[n] = '\0';
        char* slash = strrchr(path, '/');
        if (slash) { size_t len = (size_t)(slash - path); if (len < out_cap) { memcpy(out, path, len); out[len] = '\0'; return; } }
    }
#endif
    if (out_cap) { out[0] = '.'; out[1] = '\0'; }
}

/* Windows paths contain backslashes, the JSON escape character -
 * embedding one unescaped into the hand-built JSON config below would
 * corrupt it (the minimal parser's unrecognized-escape fallback takes
 * the following character literally, e.g. "\U" becomes "U"). Windows
 * accepts forward slashes in paths natively, so normalizing avoids
 * needing real JSON escaping. */
static void normalize_slashes(char* path) {
    for (char* p = path; *p; ++p) if (*p == '\\') *p = '/';
}

int main(void) {
    char exe_dir[1024];
    own_exe_dir(exe_dir, sizeof(exe_dir));
    char driver_path[1200];
    snprintf(driver_path, sizeof(driver_path), "%s/usdk_driver_fixture" USDK_PLATFORM_EXT, exe_dir);
    normalize_slashes(driver_path);
    char cfg_json[1300];
    int cn = snprintf(cfg_json, sizeof(cfg_json), "{\"driver_path\":\"%s\"}", driver_path);
    usdk_config_t cfg = { { (const uint8_t*)cfg_json, (uint32_t)cn } };

    usdk_role_instance_t* deliberate = NULL;
    if (usdk_deliberate_create(&cfg, &deliberate) != USDK_OK) {
        fprintf(stderr, "usdk_deliberate_create failed (looked for driver at %s)\n", driver_path);
        return 1;
    }

    usdk_buffer_t prompt = { (const uint8_t*)"draft a short status note", 26 };
    usdk_owned_buffer_t response = {0};
    usdk_deliberate_propose(deliberate, NULL, 0, prompt, &response);
    printf("proposed: %.*s\n", (int)response.len, (const char*)response.data);

    /* A candidate built from the response usdk_deliberate_propose just
     * generated should be accepted as self-consistent. */
    {
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "deliberate-example", 1, "cand-self-generated",
                               NULL, 0, (usdk_buffer_t){ response.data, response.len },
                               NULL, 0, 0x7fffffffffffffffLL, &candidate);
        usdk_vote_t vote;
        usdk_deliberate_vote(deliberate, &candidate, &vote);
        printf("vote on self-generated candidate: %s (%s)\n",
               vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT", vote.reason_code);
    }

    /* A candidate whose proposed_response this instance never produced
     * must be rejected - not a rubber stamp. */
    {
        usdk_buffer_t foreign_response = { (const uint8_t*)"an externally-authored response", 32 };
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "deliberate-example", 2, "cand-foreign",
                               NULL, 0, foreign_response, NULL, 0, 0x7fffffffffffffffLL, &candidate);
        usdk_vote_t vote;
        usdk_deliberate_vote(deliberate, &candidate, &vote);
        printf("vote on foreign candidate: %s (%s)\n",
               vote.verdict == USDK_VERDICT_ACCEPT ? "ACCEPT" : "REJECT", vote.reason_code);
    }

    usdk_owned_buffer_release(&response);
    usdk_deliberate_destroy(deliberate);
    return 0;
}
