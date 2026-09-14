#include "usdk/core.h"
#include "usdk/candidate.h"
#include "usdk/wire.h"
#include "journal.h"
#include <stdlib.h>
#include <string.h>

#define USDK_MAX_SESSIONS 64

struct usdk_core {
    usdk_journal_t*    journal;
    usdk_dispatch_fn_t dispatch_fn;
    void*              dispatch_user_data;
    struct {
        char     session_id[USDK_ID_LEN];
        uint64_t highest_round_id;
    } sessions[USDK_MAX_SESSIONS];
    uint32_t session_count;
};

struct usdk_round {
    usdk_candidate_t   candidate; /* header fields copied; evidence_refs/
                                    * proposed_response.data/constraints
                                    * remain borrowed from whatever the
                                    * caller passed to open_round - see
                                    * usdk/candidate.h's own borrowing
                                    * contract, which this inherits. */
    usdk_vote_t        votes[3];  /* indexed by role - 1,2,3 -> 0,1,2 */
    int                has_vote[3];
    usdk_round_state_t state;
};

static int role_index(usdk_role_t role) {
    switch (role) {
        case USDK_ROLE_PERCEIVE: return 0;
        case USDK_ROLE_DELIBERATE: return 1;
        case USDK_ROLE_VERIFY: return 2;
        default: return -1;
    }
}

static uint64_t session_highest(usdk_core_t* core, const char* session_id) {
    for (uint32_t i = 0; i < core->session_count; ++i) {
        if (strcmp(core->sessions[i].session_id, session_id) == 0) {
            return core->sessions[i].highest_round_id;
        }
    }
    return 0;
}

static void session_set_highest(usdk_core_t* core, const char* session_id, uint64_t round_id) {
    for (uint32_t i = 0; i < core->session_count; ++i) {
        if (strcmp(core->sessions[i].session_id, session_id) == 0) {
            if (round_id > core->sessions[i].highest_round_id) {
                core->sessions[i].highest_round_id = round_id;
            }
            return;
        }
    }
    if (core->session_count < USDK_MAX_SESSIONS) {
        strncpy(core->sessions[core->session_count].session_id, session_id, USDK_ID_LEN - 1);
        core->sessions[core->session_count].highest_round_id = round_id;
        core->session_count++;
    }
    /* A session count beyond USDK_MAX_SESSIONS is not tracked in memory
     * for this reference implementation; the journal (checked via
     * usdk_journal_highest_round_id below) remains the source of truth
     * across a restart regardless, so stale-round detection degrades
     * gracefully rather than silently accepting a replay - it falls back
     * to a journal scan every time instead of the in-memory cache. */
}

usdk_status_t usdk_core_create(const char* runtime_dir, usdk_core_t** out) {
    if (!runtime_dir || !out) return USDK_ERR_INVALID_ARGUMENT;
    usdk_core_t* core = (usdk_core_t*)calloc(1, sizeof(*core));
    if (!core) return USDK_ERR_OUT_OF_MEMORY;
    usdk_status_t st = usdk_journal_open(runtime_dir, &core->journal);
    if (st != USDK_OK) { free(core); return st; }
    *out = core;
    return USDK_OK;
}

void usdk_core_destroy(usdk_core_t* core) {
    if (!core) return;
    usdk_journal_close(core->journal);
    free(core);
}

void usdk_core_set_dispatch_fn(usdk_core_t* core, usdk_dispatch_fn_t fn, void* user_data) {
    if (!core) return;
    core->dispatch_fn = fn;
    core->dispatch_user_data = user_data;
}

usdk_status_t usdk_core_open_round(usdk_core_t* core, const usdk_candidate_t* candidate, usdk_round_t** out_round) {
    if (!core || !candidate || !out_round) return USDK_ERR_INVALID_ARGUMENT;
    if (candidate->struct_size < (uint32_t)sizeof(usdk_candidate_t)) return USDK_ERR_STRUCT_SIZE_MISMATCH;
    if (candidate->protocol_version != USDK_PROTOCOL_VERSION_V1) return USDK_ERR_ABI_VERSION_MISMATCH;

    uint64_t highest_mem = session_highest(core, candidate->session_id);
    uint64_t highest_journal = usdk_journal_highest_round_id(core->journal, candidate->session_id);
    uint64_t highest = highest_mem > highest_journal ? highest_mem : highest_journal;
    if (candidate->round_id <= highest) return USDK_ERR_STALE_ROUND;

    usdk_round_t* round = (usdk_round_t*)calloc(1, sizeof(*round));
    if (!round) return USDK_ERR_OUT_OF_MEMORY;
    round->candidate = *candidate;
    round->state = USDK_ROUND_OPEN;

    usdk_status_t jst = usdk_journal_append(core->journal, USDK_JOURNAL_OPEN,
                                             candidate->session_id, candidate->round_id, candidate->candidate_id);
    if (jst != USDK_OK) { free(round); return jst; }

    session_set_highest(core, candidate->session_id, candidate->round_id);
    *out_round = round;
    return USDK_OK;
}

usdk_status_t usdk_core_submit_vote(usdk_core_t* core, usdk_round_t* round, const usdk_vote_t* vote) {
    if (!core || !round || !vote) return USDK_ERR_INVALID_ARGUMENT;
    if (vote->struct_size < (uint32_t)sizeof(usdk_vote_t)) return USDK_ERR_STRUCT_SIZE_MISMATCH;
    if (round->state != USDK_ROUND_OPEN) return USDK_ERR_ROUND_NOT_OPEN;

    if (usdk_monotonic_ns() > round->candidate.deadline_ns) {
        round->state = USDK_ROUND_TIMED_OUT;
        usdk_journal_append(core->journal, USDK_JOURNAL_TIMED_OUT,
                             round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);
        return USDK_ERR_DEADLINE_EXCEEDED;
    }

    int idx = role_index(vote->role);
    if (idx < 0 || (vote->verdict != USDK_VERDICT_ACCEPT && vote->verdict != USDK_VERDICT_REJECT &&
                    vote->verdict != USDK_VERDICT_ABSTAIN)) {
        return USDK_ERR_INVALID_ARGUMENT;
    }

    if (round->has_vote[idx]) {
        if (strncmp(round->votes[idx].party_id, vote->party_id, USDK_ID_LEN) == 0) {
            return USDK_ERR_DUPLICATE_VOTE;
        }
        return USDK_ERR_PARTY_CONFLICT;
    }

    round->votes[idx] = *vote;
    round->has_vote[idx] = 1;
    return USDK_OK;
}

usdk_status_t usdk_core_round_decide(usdk_core_t* core, usdk_round_t* round, usdk_round_state_t* out_state) {
    if (!core || !round || !out_state) return USDK_ERR_INVALID_ARGUMENT;

    if (round->state != USDK_ROUND_OPEN) {
        *out_state = round->state; /* idempotent - never re-dispatches */
        return USDK_OK;
    }

    if (usdk_monotonic_ns() > round->candidate.deadline_ns) {
        round->state = USDK_ROUND_TIMED_OUT;
        usdk_journal_append(core->journal, USDK_JOURNAL_TIMED_OUT,
                             round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);
        *out_state = round->state;
        return USDK_OK;
    }

    /* Candidate mutation detection: the round's own stored candidate
     * must still hash to what it was created with (defense against a
     * caller bypassing usdk_candidate_t's lack of setters via raw memory
     * access), and every recorded vote's candidate_digest_seen must
     * match it too (a vote cast on a candidate that was since mutated
     * cannot count) - see docs/CONSENSUS_PROTOCOL.md "Candidate mutation
     * detection". */
    int mutated = !usdk_candidate_digest_matches(&round->candidate);

    int all_accept = 1;
    for (int i = 0; i < 3 && all_accept; ++i) {
        if (!round->has_vote[i]) { all_accept = 0; break; }
        if (round->votes[i].verdict != USDK_VERDICT_ACCEPT) { all_accept = 0; break; }
        if (memcmp(round->votes[i].candidate_digest_seen, round->candidate.content_digest, USDK_DIGEST_LEN) != 0) {
            all_accept = 0;
            break;
        }
    }
    if (mutated) all_accept = 0;

    if (!all_accept) {
        round->state = USDK_ROUND_REJECTED;
        usdk_journal_append(core->journal, USDK_JOURNAL_REJECTED,
                             round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);
        *out_state = round->state;
        return USDK_OK;
    }

    round->state = USDK_ROUND_COMMITTED;
    usdk_journal_append(core->journal, USDK_JOURNAL_COMMITTED,
                         round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);

    if (core->dispatch_fn) {
        usdk_status_t dst = core->dispatch_fn(core->dispatch_user_data, &round->candidate, round->candidate.candidate_id);
        usdk_journal_append(core->journal,
                             dst == USDK_OK ? USDK_JOURNAL_DISPATCHED : USDK_JOURNAL_DISPATCH_UNKNOWN,
                             round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);
    }

    *out_state = round->state;
    return USDK_OK;
}

usdk_status_t usdk_core_cancel_round(usdk_core_t* core, usdk_round_t* round) {
    if (!core || !round) return USDK_ERR_INVALID_ARGUMENT;
    if (round->state != USDK_ROUND_OPEN) return USDK_ERR_ROUND_NOT_OPEN;
    round->state = USDK_ROUND_CANCELLED;
    usdk_journal_append(core->journal, USDK_JOURNAL_CANCELLED,
                         round->candidate.session_id, round->candidate.round_id, round->candidate.candidate_id);
    return USDK_OK;
}

void usdk_core_round_destroy(usdk_round_t* round) { free(round); }

usdk_round_state_t usdk_round_get_state(const usdk_round_t* round) {
    return round ? round->state : USDK_ROUND_CANCELLED;
}

usdk_status_t usdk_core_recover_incomplete_rounds(
    usdk_core_t* core,
    usdk_incomplete_round_t* out, uint32_t out_cap, uint32_t* out_count) {
    if (!core || !out_count) return USDK_ERR_INVALID_ARGUMENT;
    *out_count = 0;

    const uint32_t cap = 8192;
    usdk_journal_record_t* records = (usdk_journal_record_t*)malloc(cap * sizeof(usdk_journal_record_t));
    if (!records) return USDK_ERR_OUT_OF_MEMORY;
    uint32_t count = 0;
    usdk_status_t st = usdk_journal_read_all(core->journal, records, cap, &count);
    if (st != USDK_OK) { free(records); return st; }

    /* Group by (session_id, round_id): track whether a terminal record
     * (COMMITTED/REJECTED/TIMED_OUT/CANCELLED) and a dispatch-outcome
     * record (DISPATCHED/DISPATCH_UNKNOWN) were seen for each. */
    typedef struct { char session_id[USDK_ID_LEN]; uint64_t round_id; char candidate_id[USDK_ID_LEN];
                      int has_terminal; int has_committed; int has_dispatch_outcome; } group_t;
    const uint32_t group_cap = 512;
    group_t* groups = (group_t*)calloc(group_cap, sizeof(group_t));
    if (!groups) { free(records); return USDK_ERR_OUT_OF_MEMORY; }
    uint32_t group_count = 0;

    for (uint32_t i = 0; i < count; ++i) {
        usdk_journal_record_t* r = &records[i];
        int gi = -1;
        for (uint32_t g = 0; g < group_count; ++g) {
            if (groups[g].round_id == r->round_id && strcmp(groups[g].session_id, r->session_id) == 0) { gi = (int)g; break; }
        }
        if (gi < 0 && group_count < group_cap) {
            gi = (int)group_count++;
            strncpy(groups[gi].session_id, r->session_id, USDK_ID_LEN - 1);
            groups[gi].round_id = r->round_id;
            strncpy(groups[gi].candidate_id, r->candidate_id, USDK_ID_LEN - 1);
        }
        if (gi < 0) continue;
        switch (r->type) {
            case USDK_JOURNAL_COMMITTED: groups[gi].has_committed = 1; groups[gi].has_terminal = 1; break;
            case USDK_JOURNAL_REJECTED:
            case USDK_JOURNAL_TIMED_OUT:
            case USDK_JOURNAL_CANCELLED: groups[gi].has_terminal = 1; break;
            case USDK_JOURNAL_DISPATCHED:
            case USDK_JOURNAL_DISPATCH_UNKNOWN: groups[gi].has_dispatch_outcome = 1; break;
            default: break;
        }
    }

    for (uint32_t g = 0; g < group_count && *out_count < out_cap; ++g) {
        int incomplete_open = !groups[g].has_terminal;
        int incomplete_dispatch = groups[g].has_committed && !groups[g].has_dispatch_outcome;
        if (incomplete_open || incomplete_dispatch) {
            usdk_incomplete_round_t* o = &out[*out_count];
            strncpy(o->session_id, groups[g].session_id, USDK_ID_LEN - 1);
            o->round_id = groups[g].round_id;
            strncpy(o->candidate_id, groups[g].candidate_id, USDK_ID_LEN - 1);
            o->dispatch_outcome_unknown = incomplete_dispatch;
            (*out_count)++;
        }
    }

    free(groups);
    free(records);
    return USDK_OK;
}
