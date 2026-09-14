#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usdk/usdk.h"
#include "../ffi/json_min.h"

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#define USDK_PLATFORM_EXT ".dll"
#elif defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#define USDK_PLATFORM_EXT ".dylib"
#else
#include <sys/stat.h>
#include <unistd.h>
#define USDK_PLATFORM_EXT ".so"
#endif

/* ----------------------------------------------------------- helpers */

static int arg_flag(int argc, char** argv, const char* name) {
    for (int i = 0; i < argc; ++i) if (strcmp(argv[i], name) == 0) return 1;
    return 0;
}
static const char* arg_str(int argc, char** argv, const char* name, const char* dflt) {
    for (int i = 0; i < argc - 1; ++i) if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return dflt;
}

static void own_exe_dir(const char* argv0, char* out, size_t out_cap) {
#if defined(_WIN32)
    char path[1024];
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    if (n > 0 && n < sizeof(path)) {
        char* slash = strrchr(path, '\\');
        if (!slash) slash = strrchr(path, '/');
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
    if (argv0) {
        const char* slash = strrchr(argv0, '/');
        const char* bslash = strrchr(argv0, '\\');
        const char* last = slash > bslash ? slash : bslash;
        if (last) { size_t len = (size_t)(last - argv0); if (len < out_cap) { memcpy(out, argv0, len); out[len] = '\0'; return; } }
    }
    if (out_cap) out[0] = '.', out[1] = '\0';
}

/* Windows paths (from own_exe_dir) contain backslashes, which are the
 * JSON escape character - embedding one unescaped into a hand-built JSON
 * string (as every usdk_config_t below does) corrupts it silently: the
 * minimal parser's unrecognized-escape fallback (src/ffi/json_min.c)
 * takes the character after an unknown "\X" literally, so "\U" becomes
 * "U" and the whole path is mangled. Windows accepts forward slashes in
 * paths natively, so normalizing avoids the need for real JSON escaping. */
static void normalize_slashes(char* path) {
    for (char* p = path; *p; ++p) if (*p == '\\') *p = '/';
}

static const char* round_state_str(usdk_round_state_t s) {
    switch (s) {
        case USDK_ROUND_OPEN: return "OPEN";
        case USDK_ROUND_COMMITTED: return "COMMITTED";
        case USDK_ROUND_REJECTED: return "REJECTED";
        case USDK_ROUND_TIMED_OUT: return "TIMED_OUT";
        case USDK_ROUND_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}
static const char* role_str(usdk_role_t r) {
    switch (r) { case USDK_ROLE_PERCEIVE: return "perceive"; case USDK_ROLE_DELIBERATE: return "deliberate";
                 case USDK_ROLE_VERIFY: return "verify"; case USDK_ROLE_DRIVER: return "driver"; default: return "unspecified"; }
}

/* ----------------------------------------------------------- usage */

static void print_usage(FILE* out) {
    fprintf(out,
        "usage: usdk <command> [options]\n"
        "commands:\n"
        "  doctor --json\n"
        "  inspect <module> [--manifest-dir DIR] [--json]\n"
        "  validate --config PATH [--json]\n"
        "  demo --scenario trilateral-consensus [--manifest-dir DIR] [--json]\n"
        "  help | --help | -h\n");
}

/* ------------------------------------------------------ role loading */

typedef struct loaded_roles {
    usdk_module_t* modules[8];
    uint32_t       module_count;
    const usdk_descriptor_t* perceive_desc;
    const usdk_role_vtable_t* perceive_vt;
    const usdk_descriptor_t* deliberate_desc;
    const usdk_role_vtable_t* deliberate_vt;
    const usdk_descriptor_t* verify_desc;
    const usdk_role_vtable_t* verify_vt;
} loaded_roles_t;

static usdk_status_t load_roles(const char* manifest_dir, loaded_roles_t* out, char* detail, uint32_t detail_cap) {
    memset(out, 0, sizeof(*out));
    const char* names[3] = {"usdk-perceive", "usdk-deliberate", "usdk-verify"};
    const usdk_descriptor_t* descs[8];
    const void* vts[8];
    usdk_status_t st = usdk_ffi_load_resolved(manifest_dir, names, 3, out->modules, descs, vts,
                                               &out->module_count, detail, detail_cap);
    if (st != USDK_OK) return st;

    for (uint32_t i = 0; i < out->module_count; ++i) {
        if (descs[i]->role == USDK_ROLE_PERCEIVE) { out->perceive_desc = descs[i]; out->perceive_vt = (const usdk_role_vtable_t*)vts[i]; }
        else if (descs[i]->role == USDK_ROLE_DELIBERATE) { out->deliberate_desc = descs[i]; out->deliberate_vt = (const usdk_role_vtable_t*)vts[i]; }
        else if (descs[i]->role == USDK_ROLE_VERIFY) { out->verify_desc = descs[i]; out->verify_vt = (const usdk_role_vtable_t*)vts[i]; }
    }
    if (!out->perceive_vt || !out->deliberate_vt || !out->verify_vt) {
        for (uint32_t i = 0; i < out->module_count; ++i) usdk_ffi_unload(out->modules[i]);
        return USDK_ERR_CAPABILITY_NOT_FOUND;
    }
    return USDK_OK;
}

static void unload_roles(loaded_roles_t* r) {
    for (uint32_t i = 0; i < r->module_count; ++i) usdk_ffi_unload(r->modules[i]);
}

/* ----------------------------------------------------------- doctor */

static int cmd_doctor(int argc, char** argv) {
    int as_json = arg_flag(argc, argv, "--json");
    char exe_dir[1024];
    own_exe_dir(argv[0], exe_dir, sizeof(exe_dir));

    char detail[256] = {0};
    loaded_roles_t roles;
    usdk_status_t st = load_roles(exe_dir, &roles, detail, sizeof(detail));
    int ok = (st == USDK_OK);
    if (ok) unload_roles(&roles);

    if (as_json) {
        printf("{\"checks\":[{\"name\":\"core_library\",\"status\":\"ok\",\"detail\":\"usdk %s ABI %u.%u\"},",
               "0.1.0", USDK_ABI_VERSION_MAJOR, USDK_ABI_VERSION_MINOR);
        printf("{\"name\":\"role_modules\",\"status\":\"%s\",\"detail\":\"%s\"}],\"overall\":\"%s\"}\n",
               ok ? "ok" : "fail", ok ? "perceive/deliberate/verify (and their dependencies) loaded and ABI-validated" : detail,
               ok ? "ok" : "fail");
    } else {
        printf("core_library: ok (usdk 0.1.0, ABI %u.%u)\n", USDK_ABI_VERSION_MAJOR, USDK_ABI_VERSION_MINOR);
        printf("role_modules: %s\n", ok ? "ok" : detail);
        printf("overall: %s\n", ok ? "ok" : "fail");
    }
    return ok ? 0 : 1;
}

/* ----------------------------------------------------------- inspect */

static int cmd_inspect(int argc, char** argv) {
    int as_json = arg_flag(argc, argv, "--json");
    const char* module_name = (argc >= 3 && argv[2][0] != '-') ? argv[2] : NULL;
    if (!module_name) {
        fprintf(stderr, "usdk inspect: a module name is required\n");
        return 2;
    }
    char exe_dir[1024];
    own_exe_dir(argv[0], exe_dir, sizeof(exe_dir));
    const char* manifest_dir = arg_str(argc, argv, "--manifest-dir", exe_dir);

    const char* names[1] = { module_name };
    usdk_module_t* modules[1];
    const usdk_descriptor_t* descs[1];
    const void* vts[1];
    uint32_t count = 0;
    char detail[256] = {0};
    usdk_status_t st = usdk_ffi_load_resolved(manifest_dir, names, 1, modules, descs, vts, &count, detail, sizeof(detail));
    if (st != USDK_OK) {
        if (as_json) printf("{\"module\":\"%s\",\"found\":false,\"error\":\"%s\"}\n", module_name, usdk_status_string(st));
        else fprintf(stderr, "usdk inspect: %s: %s (%s)\n", module_name, usdk_status_string(st), detail);
        return 1;
    }
    /* `count` may exceed 1: resolving a single requested capability still
     * transitively loads its dependencies (e.g. "usdk-deliberate" pulls
     * in "usdk-driver-fixture"). Report the descriptor whose declared
     * party_id actually matches what was asked for, not whichever module
     * happened to load last. */
    const usdk_descriptor_t* d = descs[count - 1];
    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(descs[i]->party_id, module_name) == 0) { d = descs[i]; break; }
    }
    if (as_json) {
        printf("{\"module\":\"%s\",\"found\":true,\"role\":\"%s\",\"party_id\":\"%s\","
               "\"abi_version\":\"%u.%u\",\"capability_flags\":%u}\n",
               module_name, role_str(d->role), d->party_id, d->abi_version_major, d->abi_version_minor, d->capability_flags);
    } else {
        printf("module: %s\nrole: %s\nparty_id: %s\nabi_version: %u.%u\ncapability_flags: %u\n",
               module_name, role_str(d->role), d->party_id, d->abi_version_major, d->abi_version_minor, d->capability_flags);
    }
    for (uint32_t i = 0; i < count; ++i) usdk_ffi_unload(modules[i]);
    return 0;
}

/* ----------------------------------------------------------- validate */

static int cmd_validate(int argc, char** argv) {
    int as_json = arg_flag(argc, argv, "--json");
    const char* config_path = arg_str(argc, argv, "--config", NULL);
    if (!config_path) {
        fprintf(stderr, "usdk validate: --config PATH is required\n");
        return 2;
    }
    FILE* f = fopen(config_path, "rb");
    if (!f) {
        fprintf(stderr, "usdk validate: cannot open %s\n", config_path);
        return 1;
    }
    uint8_t buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    char manifest_dir[1024] = {0};
    const char* p = (const char*)buf;
    const char* end = p + n;
    const char* v = usdk_json_find_key(p, end, "manifest_dir");
    if (!v || !usdk_json_parse_string(v, end, manifest_dir, sizeof(manifest_dir))) {
        if (as_json) printf("{\"overall\":\"fail\",\"error\":\"config missing 'manifest_dir'\"}\n");
        else fprintf(stderr, "validate: config missing 'manifest_dir'\n");
        return 1;
    }

    loaded_roles_t roles;
    char detail[256] = {0};
    usdk_status_t st = load_roles(manifest_dir, &roles, detail, sizeof(detail));
    int ok = (st == USDK_OK);
    if (ok) unload_roles(&roles);

    if (as_json) {
        printf("{\"manifest_dir\":\"%s\",\"perceive\":%s,\"deliberate\":%s,\"verify\":%s,\"overall\":\"%s\"}\n",
               manifest_dir, ok ? "true" : "false", ok ? "true" : "false", ok ? "true" : "false", ok ? "ok" : "fail");
    } else {
        printf("manifest_dir: %s\nperceive: %s\ndeliberate: %s\nverify: %s\noverall: %s\n",
               manifest_dir, ok ? "found" : "missing", ok ? "found" : "missing", ok ? "found" : "missing", ok ? "ok" : "fail");
    }
    return ok ? 0 : 1;
}

/* ----------------------------------------------------------- demo */

typedef struct scenario_result {
    const char* name;
    usdk_round_state_t state;
    char detail[256];
} scenario_result_t;

/* Runs perceive/deliberate/verify votes against `candidate` through
 * `core`/`round` and returns the round's final state - the shared inner
 * loop every demo scenario below uses.
 *
 * The demo links usdk-perceive/usdk-deliberate/usdk-verify directly
 * (see src/cli/CMakeLists.txt) and calls their full public API
 * (usdk/perceive.h etc.), including usdk_perceive_observe, which has no
 * vtable equivalent - usdk_role_vtable_t only standardizes create/
 * destroy/vote(/propose for deliberate), the operations every party
 * needs to participate in a round, not every operation a specific role
 * happens to expose. The dynamic-loading path (usdk-ffi, manifest
 * resolution) is exercised instead by `usdk doctor`/`inspect`/
 * `validate` (see load_roles() above), which is where genuinely
 * discovering an arbitrary named module belongs - see
 * docs/GETTING_STARTED.md for both patterns spelled out. */
static usdk_round_state_t run_round(usdk_core_t* core, usdk_round_t* round,
                                     usdk_role_instance_t* perceive_inst, usdk_role_instance_t* deliberate_inst,
                                     usdk_role_instance_t* verify_inst, const usdk_candidate_t* candidate,
                                     int tamper_deliberate_vote) {
    usdk_vote_t vote;

    if (usdk_perceive_vote(perceive_inst, candidate, &vote) == USDK_OK) usdk_core_submit_vote(core, round, &vote);
    if (usdk_deliberate_vote(deliberate_inst, candidate, &vote) == USDK_OK) {
        if (tamper_deliberate_vote) memset(vote.candidate_digest_seen, 0, USDK_DIGEST_LEN);
        usdk_core_submit_vote(core, round, &vote);
    }
    if (usdk_verify_vote(verify_inst, candidate, &vote) == USDK_OK) usdk_core_submit_vote(core, round, &vote);

    usdk_round_state_t state;
    usdk_core_round_decide(core, round, &state);
    return state;
}

static usdk_status_t dispatch_write_file(void* user_data, const usdk_candidate_t* c, const char* idempotency_key) {
    const char* workspace = (const char*)user_data;
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.txt", workspace, idempotency_key);
    /* Idempotent by construction: writing the exact same bytes for the
     * same idempotency_key converges to the same file content no matter
     * how many times this is called - see
     * docs/CONSENSUS_PROTOCOL.md "Idempotency requirements for tool
     * execution". */
    FILE* f = fopen(path, "wb");
    if (!f) return USDK_ERR_IO;
    fwrite(c->proposed_response.data, 1, c->proposed_response.len, f);
    fclose(f);
    return USDK_OK;
}

static int cmd_demo(int argc, char** argv) {
    int as_json = arg_flag(argc, argv, "--json");
    const char* scenario = arg_str(argc, argv, "--scenario", NULL);
    if (!scenario || strcmp(scenario, "trilateral-consensus") != 0) {
        fprintf(stderr, "usdk demo: --scenario trilateral-consensus is required (only scenario implemented)\n");
        return 2;
    }
    char exe_dir[1024];
    own_exe_dir(argv[0], exe_dir, sizeof(exe_dir));
    /* Only used to locate usdk_driver_fixture - the demo links the three
     * role libraries directly (see the comment on run_round() above), so
     * no manifest resolution happens for perceive/deliberate/verify
     * themselves. */
    const char* driver_dir = arg_str(argc, argv, "--manifest-dir", exe_dir);

    /* PID alone is not enough here: the demo always uses the same fixed
     * session_id/round_ids every invocation (see the scenarios below), so
     * if the OS ever reuses a PID across two separate `usdk demo` runs -
     * plausible across many CI iterations, not just hypothetical - the
     * second run would find the first run's journal already OPENed those
     * exact round_ids and get rejected as a replay (see
     * docs/CONSENSUS_PROTOCOL.md "Stale rounds and replay rejection").
     * usdk_monotonic_ns() added alongside the PID makes the directory
     * unique per invocation regardless. */
    char runtime_dir[1200];
    snprintf(runtime_dir, sizeof(runtime_dir), "./usdk-demo-run-%ld-%lld",
#if defined(_WIN32)
             (long)GetCurrentProcessId(),
#else
             (long)getpid(),
#endif
             (long long)usdk_monotonic_ns()
    );
#if defined(_WIN32)
    _mkdir(runtime_dir);
#else
    mkdir(runtime_dir, 0755);
#endif

    char driver_path[1200];
    snprintf(driver_path, sizeof(driver_path), "%s/usdk_driver_fixture" USDK_PLATFORM_EXT, driver_dir);
    normalize_slashes(driver_path); /* see the comment on normalize_slashes() - this path is about to be embedded in JSON */
    char deliberate_cfg_json[1300];
    int dcn = snprintf(deliberate_cfg_json, sizeof(deliberate_cfg_json), "{\"driver_path\":\"%s\"}", driver_path);
    usdk_config_t deliberate_cfg = { { (const uint8_t*)deliberate_cfg_json, (uint32_t)dcn } };

    const char* verify_cfg_json = "{\"allowed_constraints\":[\"workspace:temp-only\"],\"max_uncertainty\":0.5}";
    usdk_config_t verify_cfg = { { (const uint8_t*)verify_cfg_json, (uint32_t)strlen(verify_cfg_json) } };

    usdk_role_instance_t *perceive_inst = NULL, *deliberate_inst = NULL, *verify_inst = NULL;
    usdk_status_t st = usdk_perceive_create(NULL, &perceive_inst);
    if (st == USDK_OK) st = usdk_deliberate_create(&deliberate_cfg, &deliberate_inst);
    if (st == USDK_OK) st = usdk_verify_create(&verify_cfg, &verify_inst);
    if (st != USDK_OK) {
        fprintf(stderr, "usdk demo: failed to create role instances: %s\n", usdk_status_string(st));
        if (perceive_inst) usdk_perceive_destroy(perceive_inst);
        if (deliberate_inst) usdk_deliberate_destroy(deliberate_inst);
        return 1;
    }

    usdk_core_t* core = NULL;
    if (usdk_core_create(runtime_dir, &core) != USDK_OK) {
        fprintf(stderr, "usdk demo: failed to create core\n");
        return 1;
    }
    usdk_core_set_dispatch_fn(core, dispatch_write_file, runtime_dir);

    scenario_result_t results[4];
    uint32_t nres = 0;

    /* --- Scenario 1: unanimous accept -> commit. --------------------- */
    {
        usdk_buffer_t raw = { (const uint8_t*)"sensor: temperature 72F, stable", 32 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive_inst, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"summarize the sensor evidence", 30 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate_inst, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "demo-session", 1, "cand-accept",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() + 5000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &candidate, &round);
        usdk_round_state_t state = run_round(core, round, perceive_inst, deliberate_inst, verify_inst, &candidate, 0);
        results[nres].name = "unanimous_accept_commits";
        results[nres].state = state;
        snprintf(results[nres].detail, sizeof(results[nres].detail), "expected COMMITTED, got %s", round_state_str(state));
        nres++;
        usdk_core_round_destroy(round);
        usdk_owned_buffer_release(&response);
    }

    /* --- Scenario 2: insufficient permission -> reject. -------------- */
    {
        usdk_buffer_t raw = { (const uint8_t*)"sensor: pressure nominal", 25 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive_inst, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"propose a disallowed action", 28 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate_inst, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "action:delete-all" }}; /* not in verify's allow-list */
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "demo-session", 2, "cand-reject-permission",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() + 5000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &candidate, &round);
        usdk_round_state_t state = run_round(core, round, perceive_inst, deliberate_inst, verify_inst, &candidate, 0);
        results[nres].name = "insufficient_permission_rejects";
        results[nres].state = state;
        snprintf(results[nres].detail, sizeof(results[nres].detail), "expected REJECTED (verify has no 'action:delete-all' in its allow-list), got %s", round_state_str(state));
        nres++;
        usdk_core_round_destroy(round);
        usdk_owned_buffer_release(&response);
    }

    /* --- Scenario 3: deadline already passed -> timed out. ----------- */
    {
        usdk_buffer_t raw = { (const uint8_t*)"sensor: humidity 40%", 21 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive_inst, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"summarize", 9 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate_inst, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        /* Deadline already in the past - simulates an unavailable/too-slow
         * party without needing an actual hung call. */
        usdk_candidate_create(1, "demo-session", 3, "cand-timeout",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() - 1000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_status_t open_st = usdk_core_open_round(core, &candidate, &round);
        usdk_round_state_t state;
        if (open_st == USDK_OK) {
            usdk_core_round_decide(core, round, &state);
            usdk_core_round_destroy(round);
        } else {
            state = USDK_ROUND_TIMED_OUT; /* opened at all is enough to prove nothing committed */
        }
        results[nres].name = "expired_deadline_times_out";
        results[nres].state = state;
        snprintf(results[nres].detail, sizeof(results[nres].detail), "expected TIMED_OUT, got %s", round_state_str(state));
        nres++;
        usdk_owned_buffer_release(&response);
    }

    /* --- Scenario 4: a vote's recorded digest does not match the round's
     * candidate (simulated mutation) -> cannot commit. ----------------- */
    {
        usdk_buffer_t raw = { (const uint8_t*)"sensor: vibration normal", 25 };
        usdk_evidence_ref_t ev;
        usdk_perceive_observe(perceive_inst, raw, 0.1, &ev);

        usdk_buffer_t prompt = { (const uint8_t*)"summarize", 9 };
        usdk_owned_buffer_t response = {0};
        usdk_deliberate_propose(deliberate_inst, &ev, 1, prompt, &response);

        usdk_constraint_t constraints[1] = {{ "workspace:temp-only" }};
        usdk_candidate_t candidate;
        usdk_candidate_create(1, "demo-session", 4, "cand-mutated",
                               &ev, 1, (usdk_buffer_t){ response.data, response.len },
                               constraints, 1, usdk_monotonic_ns() + 5000000000LL, &candidate);

        usdk_round_t* round = NULL;
        usdk_core_open_round(core, &candidate, &round);
        usdk_round_state_t state = run_round(core, round, perceive_inst, deliberate_inst, verify_inst, &candidate, 1 /* tamper */);
        results[nres].name = "mutated_vote_cannot_commit";
        results[nres].state = state;
        snprintf(results[nres].detail, sizeof(results[nres].detail), "expected REJECTED (deliberate's recorded vote digest was tampered), got %s", round_state_str(state));
        nres++;
        usdk_core_round_destroy(round);
        usdk_owned_buffer_release(&response);
    }

    usdk_perceive_destroy(perceive_inst);
    usdk_deliberate_destroy(deliberate_inst);
    usdk_verify_destroy(verify_inst);
    usdk_core_destroy(core);

    int overall_ok = (results[0].state == USDK_ROUND_COMMITTED) &&
                      (results[1].state == USDK_ROUND_REJECTED) &&
                      (results[2].state == USDK_ROUND_TIMED_OUT) &&
                      (results[3].state == USDK_ROUND_REJECTED);

    if (as_json) {
        printf("{\"scenario\":\"trilateral-consensus\",\"runtime_dir\":\"%s\",\"results\":[", runtime_dir);
        for (uint32_t i = 0; i < nres; ++i) {
            printf("%s{\"name\":\"%s\",\"round_state\":\"%s\"}", i > 0 ? "," : "", results[i].name, round_state_str(results[i].state));
        }
        printf("],\"overall\":\"%s\"}\n", overall_ok ? "pass" : "fail");
    } else {
        for (uint32_t i = 0; i < nres; ++i) printf("%s: %s\n", results[i].name, round_state_str(results[i].state));
        printf("overall: %s\n", overall_ok ? "pass" : "fail");
    }
    return overall_ok ? 0 : 1;
}

/* ----------------------------------------------------------- main */

/* Every cmd_* function above receives the FULL, unshifted (argc, argv)
 * from main() - argv[0] is always the program path (needed by
 * own_exe_dir), argv[1] is always the command name, and arg_flag/arg_str
 * scan the whole array (harmless: a flag name never collides with the
 * program path or command name). This one convention, used uniformly, is
 * deliberate - a prior draft of this dispatch shifted argv for some
 * commands but not others, which left cmd_doctor reading the command
 * name itself as if it were the program path. */
int main(int argc, char** argv) {
    if (argc < 2) { print_usage(stderr); return 2; }
    const char* cmd = argv[1];
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (strcmp(cmd, "doctor") == 0) return cmd_doctor(argc, argv);
    if (strcmp(cmd, "inspect") == 0) return cmd_inspect(argc, argv);
    if (strcmp(cmd, "validate") == 0) return cmd_validate(argc, argv);
    if (strcmp(cmd, "demo") == 0) return cmd_demo(argc, argv);
    fprintf(stderr, "usdk: unknown command '%s'\n", cmd);
    print_usage(stderr);
    return 2;
}
