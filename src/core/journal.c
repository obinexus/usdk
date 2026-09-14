#include "journal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define USDK_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define USDK_MKDIR(path) mkdir(path, 0755)
#endif

struct usdk_journal {
    char path[1024];
};

static const char* type_name(usdk_journal_record_type_t t) {
    switch (t) {
        case USDK_JOURNAL_OPEN: return "OPEN";
        case USDK_JOURNAL_COMMITTED: return "COMMITTED";
        case USDK_JOURNAL_REJECTED: return "REJECTED";
        case USDK_JOURNAL_TIMED_OUT: return "TIMED_OUT";
        case USDK_JOURNAL_CANCELLED: return "CANCELLED";
        case USDK_JOURNAL_DISPATCHED: return "DISPATCHED";
        case USDK_JOURNAL_DISPATCH_UNKNOWN: return "DISPATCH_UNKNOWN";
        default: return "UNKNOWN";
    }
}

static int parse_type(const char* s, usdk_journal_record_type_t* out) {
    if (strcmp(s, "OPEN") == 0) { *out = USDK_JOURNAL_OPEN; return 1; }
    if (strcmp(s, "COMMITTED") == 0) { *out = USDK_JOURNAL_COMMITTED; return 1; }
    if (strcmp(s, "REJECTED") == 0) { *out = USDK_JOURNAL_REJECTED; return 1; }
    if (strcmp(s, "TIMED_OUT") == 0) { *out = USDK_JOURNAL_TIMED_OUT; return 1; }
    if (strcmp(s, "CANCELLED") == 0) { *out = USDK_JOURNAL_CANCELLED; return 1; }
    if (strcmp(s, "DISPATCHED") == 0) { *out = USDK_JOURNAL_DISPATCHED; return 1; }
    if (strcmp(s, "DISPATCH_UNKNOWN") == 0) { *out = USDK_JOURNAL_DISPATCH_UNKNOWN; return 1; }
    return 0;
}

usdk_status_t usdk_journal_open(const char* runtime_dir, usdk_journal_t** out) {
    if (!runtime_dir || !out) return USDK_ERR_INVALID_ARGUMENT;
    USDK_MKDIR(runtime_dir); /* ignore failure - either it now exists, or
                               * the fopen below will fail and report it */
    usdk_journal_t* j = (usdk_journal_t*)calloc(1, sizeof(*j));
    if (!j) return USDK_ERR_OUT_OF_MEMORY;
    int n = snprintf(j->path, sizeof(j->path), "%s/usdk-round-journal.log", runtime_dir);
    if (n < 0 || (size_t)n >= sizeof(j->path)) { free(j); return USDK_ERR_INVALID_ARGUMENT; }

    /* Touch the file into existence so a fresh runtime_dir has an empty,
     * readable journal rather than "file not found" being ambiguous with
     * "journal not yet written to." */
    FILE* f = fopen(j->path, "ab");
    if (!f) { free(j); return USDK_ERR_IO; }
    fclose(f);

    *out = j;
    return USDK_OK;
}

void usdk_journal_close(usdk_journal_t* j) { free(j); }

usdk_status_t usdk_journal_append(usdk_journal_t* j, usdk_journal_record_type_t type,
                                   const char* session_id, uint64_t round_id,
                                   const char* candidate_id) {
    if (!j || !session_id || !candidate_id) return USDK_ERR_INVALID_ARGUMENT;
    FILE* f = fopen(j->path, "ab");
    if (!f) return USDK_ERR_IO;
    /* One record per line; fields are space-separated and none of
     * session_id/candidate_id may legally contain whitespace (they are
     * caller-supplied ids, documented as opaque tokens, not free text). */
    fprintf(f, "%s %s %llu %s\n", type_name(type), session_id,
            (unsigned long long)round_id, candidate_id);
    fclose(f);
    return USDK_OK;
}

usdk_status_t usdk_journal_read_all(usdk_journal_t* j, usdk_journal_record_t* out,
                                     uint32_t out_cap, uint32_t* out_count) {
    if (!j || !out || !out_count) return USDK_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    FILE* f = fopen(j->path, "rb");
    if (!f) return USDK_OK; /* nothing written yet */
    char line[512];
    while (*out_count < out_cap && fgets(line, sizeof(line), f)) {
        char type_str[32], session_id[USDK_ID_LEN], candidate_id[USDK_ID_LEN];
        unsigned long long round_id = 0;
        int matched = sscanf(line, "%31s %63s %llu %63s", type_str, session_id, &round_id, candidate_id);
        if (matched != 4) continue;
        usdk_journal_record_type_t t;
        if (!parse_type(type_str, &t)) continue;
        usdk_journal_record_t* r = &out[*out_count];
        r->type = t;
        strncpy(r->session_id, session_id, USDK_ID_LEN - 1); r->session_id[USDK_ID_LEN-1] = '\0';
        r->round_id = (uint64_t)round_id;
        strncpy(r->candidate_id, candidate_id, USDK_ID_LEN - 1); r->candidate_id[USDK_ID_LEN-1] = '\0';
        (*out_count)++;
    }
    fclose(f);
    return USDK_OK;
}

uint64_t usdk_journal_highest_round_id(usdk_journal_t* j, const char* session_id) {
    const uint32_t cap = 4096;
    usdk_journal_record_t* records = (usdk_journal_record_t*)malloc(cap * sizeof(usdk_journal_record_t));
    if (!records) return 0;
    uint32_t count = 0;
    if (usdk_journal_read_all(j, records, cap, &count) != USDK_OK) { free(records); return 0; }
    uint64_t highest = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (records[i].type == USDK_JOURNAL_OPEN &&
            strcmp(records[i].session_id, session_id) == 0 &&
            records[i].round_id > highest) {
            highest = records[i].round_id;
        }
    }
    free(records);
    return highest;
}
