#include "usdk/manifest.h"
#include "json_min.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

static void set_detail(char* detail, uint32_t cap, const char* msg) {
    if (detail && cap) { strncpy(detail, msg, cap - 1); detail[cap - 1] = '\0'; }
}

static usdk_status_t parse_role(const char* s, usdk_role_t* out) {
    if (strcmp(s, "perceive") == 0) { *out = USDK_ROLE_PERCEIVE; return USDK_OK; }
    if (strcmp(s, "deliberate") == 0) { *out = USDK_ROLE_DELIBERATE; return USDK_OK; }
    if (strcmp(s, "verify") == 0) { *out = USDK_ROLE_VERIFY; return USDK_OK; }
    if (strcmp(s, "driver") == 0) { *out = USDK_ROLE_DRIVER; return USDK_OK; }
    return USDK_ERR_INVALID_ARGUMENT;
}

usdk_status_t usdk_manifest_parse(const uint8_t* json, uint32_t len, usdk_manifest_t* out,
                                   char* detail, uint32_t detail_cap) {
    if (!json || !out) { set_detail(detail, detail_cap, "null argument"); return USDK_ERR_INVALID_ARGUMENT; }
    memset(out, 0, sizeof(*out));
    const char* p = (const char*)json;
    const char* end = p + len;

    const char* v;
    char role_str[32];

    v = usdk_json_find_key(p, end, "name");
    if (!v || !usdk_json_parse_string(v, end, out->name, sizeof(out->name))) {
        set_detail(detail, detail_cap, "manifest missing or invalid 'name'"); return USDK_ERR_INVALID_ARGUMENT;
    }
    v = usdk_json_find_key(p, end, "version_major");
    if (!v || !usdk_json_parse_uint(v, end, &out->version_major)) {
        set_detail(detail, detail_cap, "manifest missing or invalid 'version_major'"); return USDK_ERR_INVALID_ARGUMENT;
    }
    v = usdk_json_find_key(p, end, "version_minor");
    if (!v || !usdk_json_parse_uint(v, end, &out->version_minor)) {
        set_detail(detail, detail_cap, "manifest missing or invalid 'version_minor'"); return USDK_ERR_INVALID_ARGUMENT;
    }
    v = usdk_json_find_key(p, end, "role");
    if (!v || !usdk_json_parse_string(v, end, role_str, sizeof(role_str)) || parse_role(role_str, &out->role) != USDK_OK) {
        set_detail(detail, detail_cap, "manifest missing or invalid 'role'"); return USDK_ERR_INVALID_ARGUMENT;
    }
    v = usdk_json_find_key(p, end, "artifact_path");
    if (!v || !usdk_json_parse_string(v, end, out->artifact_path, sizeof(out->artifact_path))) {
        set_detail(detail, detail_cap, "manifest missing or invalid 'artifact_path'"); return USDK_ERR_INVALID_ARGUMENT;
    }

    /* "dependencies" is optional. */
    v = usdk_json_find_key(p, end, "dependencies");
    if (v && v < end && *v == '[') {
        const char* cur = v + 1;
        out->dependency_count = 0;
        for (;;) {
            cur = usdk_json_skip_ws(cur, end);
            if (cur >= end) { set_detail(detail, detail_cap, "unterminated dependencies array"); return USDK_ERR_INVALID_ARGUMENT; }
            if (*cur == ']') { ++cur; break; }
            if (*cur == ',') { ++cur; continue; }
            if (*cur != '{') { set_detail(detail, detail_cap, "dependencies entries must be objects"); return USDK_ERR_INVALID_ARGUMENT; }
            const char* obj_start = cur;
            const char* obj_end = usdk_json_skip_value(obj_start, end);
            if (!obj_end) { set_detail(detail, detail_cap, "malformed dependency object"); return USDK_ERR_INVALID_ARGUMENT; }
            if (out->dependency_count >= USDK_MANIFEST_MAX_DEPS) {
                set_detail(detail, detail_cap, "too many dependencies (USDK_MANIFEST_MAX_DEPS)");
                return USDK_ERR_INVALID_ARGUMENT;
            }
            usdk_dependency_ref_t* dep = &out->dependencies[out->dependency_count];
            memset(dep, 0, sizeof(*dep));
            const char* cv = usdk_json_find_key(obj_start, obj_end, "capability");
            if (!cv || !usdk_json_parse_string(cv, obj_end, dep->capability, sizeof(dep->capability))) {
                set_detail(detail, detail_cap, "dependency missing 'capability'"); return USDK_ERR_INVALID_ARGUMENT;
            }
            const char* mv = usdk_json_find_key(obj_start, obj_end, "min_version_major");
            if (mv) usdk_json_parse_uint(mv, obj_end, &dep->min_version_major);
            mv = usdk_json_find_key(obj_start, obj_end, "min_version_minor");
            if (mv) usdk_json_parse_uint(mv, obj_end, &dep->min_version_minor);
            out->dependency_count++;
            cur = obj_end;
        }
    }

    return USDK_OK;
}

usdk_status_t usdk_manifest_load_dir(const char* dir, usdk_manifest_set_t* out,
                                      char* detail, uint32_t detail_cap) {
    if (!dir || !out) { set_detail(detail, detail_cap, "null argument"); return USDK_ERR_INVALID_ARGUMENT; }
    memset(out, 0, sizeof(*out));

#if defined(_WIN32)
    char pattern[USDK_MANIFEST_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*.usdk-manifest.json", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return USDK_OK; /* no manifests found - not an error */
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (out->count >= USDK_MANIFEST_SET_MAX) break;
        char path[USDK_MANIFEST_PATH_LEN];
        snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        uint8_t buf[8192];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        char local_detail[128];
        if (usdk_manifest_parse(buf, (uint32_t)n, &out->entries[out->count], local_detail, sizeof(local_detail)) == USDK_OK) {
            out->count++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return USDK_OK;
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        const char* name = de->d_name;
        size_t nlen = strlen(name);
        const char* suffix = ".usdk-manifest.json";
        size_t slen = strlen(suffix);
        if (nlen <= slen || strcmp(name + nlen - slen, suffix) != 0) continue;
        if (out->count >= USDK_MANIFEST_SET_MAX) break;
        char path[USDK_MANIFEST_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        uint8_t buf[8192];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        char local_detail[128];
        if (usdk_manifest_parse(buf, (uint32_t)n, &out->entries[out->count], local_detail, sizeof(local_detail)) == USDK_OK) {
            out->count++;
        }
    }
    closedir(d);
#endif
    return USDK_OK;
}

static const usdk_manifest_t* find_by_name(const usdk_manifest_set_t* set, const char* name) {
    for (uint32_t i = 0; i < set->count; ++i) {
        if (strcmp(set->entries[i].name, name) == 0) return &set->entries[i];
    }
    return NULL;
}

/* Kahn's algorithm over the complete transitive dependency graph reachable
 * from `requested_names` within `set` - see docs/ABI.md "Module manifests
 * and dependency resolution" for why this is a real topological sort, not
 * a shortest-path substitute. */
usdk_status_t usdk_manifest_resolve(
    const usdk_manifest_set_t* set,
    const char* const* requested_names, uint32_t requested_count,
    usdk_resolution_plan_t* out_plan,
    char* detail, uint32_t detail_cap) {
    if (!set || !requested_names || !out_plan) { set_detail(detail, detail_cap, "null argument"); return USDK_ERR_INVALID_ARGUMENT; }
    memset(out_plan, 0, sizeof(*out_plan));

    /* 1. Collect the reachable node set (BFS from the requested roots). */
    const usdk_manifest_t* nodes[USDK_RESOLUTION_MAX];
    uint32_t node_count = 0;
    for (uint32_t i = 0; i < requested_count; ++i) {
        const usdk_manifest_t* m = find_by_name(set, requested_names[i]);
        if (!m) { set_detail(detail, detail_cap, requested_names[i]); return USDK_ERR_DEPENDENCY_UNRESOLVED; }
        int already = 0;
        for (uint32_t k = 0; k < node_count; ++k) if (nodes[k] == m) { already = 1; break; }
        if (!already) {
            if (node_count >= USDK_RESOLUTION_MAX) { set_detail(detail, detail_cap, "resolution set too large"); return USDK_ERR_DEPENDENCY_UNRESOLVED; }
            nodes[node_count++] = m;
        }
    }
    for (uint32_t i = 0; i < node_count; ++i) {
        const usdk_manifest_t* m = nodes[i];
        for (uint32_t d = 0; d < m->dependency_count; ++d) {
            const usdk_manifest_t* dep = find_by_name(set, m->dependencies[d].capability);
            if (!dep) { set_detail(detail, detail_cap, m->dependencies[d].capability); return USDK_ERR_DEPENDENCY_UNRESOLVED; }
            if (dep->version_major < m->dependencies[d].min_version_major ||
                (dep->version_major == m->dependencies[d].min_version_major &&
                 dep->version_minor < m->dependencies[d].min_version_minor)) {
                set_detail(detail, detail_cap, dep->name);
                return USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE;
            }
            int already = 0;
            for (uint32_t k = 0; k < node_count; ++k) if (nodes[k] == dep) { already = 1; break; }
            if (!already) {
                if (node_count >= USDK_RESOLUTION_MAX) { set_detail(detail, detail_cap, "resolution set too large"); return USDK_ERR_DEPENDENCY_UNRESOLVED; }
                nodes[node_count++] = dep;
                /* nodes may grow while iterating i < node_count below via
                 * the outer for-loop bound re-read each iteration - this
                 * is intentional: it lets a dependency-of-a-dependency be
                 * discovered and still be visited by this same pass over
                 * a fixed small array, rather than requiring recursion. */
            }
        }
    }

    /* 2. Kahn's algorithm: compute in-degree (count of dependencies still
     * unscheduled) for each node, repeatedly emit a node with in-degree
     * 0, decrementing the in-degree of nodes that depend on it. */
    uint32_t in_degree[USDK_RESOLUTION_MAX];
    for (uint32_t i = 0; i < node_count; ++i) in_degree[i] = nodes[i]->dependency_count;

    int emitted[USDK_RESOLUTION_MAX];
    memset(emitted, 0, sizeof(emitted));
    uint32_t emitted_count = 0;

    while (emitted_count < node_count) {
        int progressed = 0;
        for (uint32_t i = 0; i < node_count; ++i) {
            if (emitted[i] || in_degree[i] != 0) continue;
            out_plan->order[out_plan->count++] = nodes[i];
            emitted[i] = 1;
            emitted_count++;
            progressed = 1;
            for (uint32_t j = 0; j < node_count; ++j) {
                if (emitted[j]) continue;
                for (uint32_t d = 0; d < nodes[j]->dependency_count; ++d) {
                    if (strcmp(nodes[j]->dependencies[d].capability, nodes[i]->name) == 0) {
                        if (in_degree[j] > 0) in_degree[j]--;
                    }
                }
            }
        }
        if (!progressed) {
            set_detail(detail, detail_cap, "dependency cycle detected");
            return USDK_ERR_DEPENDENCY_CYCLE;
        }
    }

    return USDK_OK;
}
