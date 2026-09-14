#ifndef USDK_MANIFEST_H
#define USDK_MANIFEST_H

#include "usdk/types.h"
#include "usdk/status.h"

USDK_BEGIN_DECLS

#define USDK_MANIFEST_MAX_DEPS 16
#define USDK_MANIFEST_PATH_LEN 512

typedef struct usdk_dependency_ref {
    char     capability[USDK_ID_LEN]; /* the depended-on capability/module name */
    uint32_t min_version_major;
    uint32_t min_version_minor;
} usdk_dependency_ref_t;

/* Parsed form of a module's manifest JSON. See docs/ABI.md "Module
 * manifests and dependency resolution". */
typedef struct usdk_manifest {
    char     name[USDK_ID_LEN];
    uint32_t version_major;
    uint32_t version_minor;
    usdk_role_t role;
    char     artifact_path[USDK_MANIFEST_PATH_LEN];
    usdk_dependency_ref_t dependencies[USDK_MANIFEST_MAX_DEPS];
    uint32_t dependency_count;
} usdk_manifest_t;

/* Parses a manifest from `json`/`len` into `out`. `detail`/`detail_cap`
 * receive a short human-readable parse-failure reason on error (always
 * NUL-terminated if detail_cap > 0). */
USDK_EXPORT usdk_status_t USDK_CALL usdk_manifest_parse(
    const uint8_t* json, uint32_t len, usdk_manifest_t* out,
    char* detail, uint32_t detail_cap);

#define USDK_MANIFEST_SET_MAX 64

typedef struct usdk_manifest_set {
    usdk_manifest_t entries[USDK_MANIFEST_SET_MAX];
    uint32_t        count;
} usdk_manifest_set_t;

/* Loads every "*.usdk-manifest.json" file directly inside `dir` (not
 * recursive) into `out`. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_manifest_load_dir(
    const char* dir, usdk_manifest_set_t* out, char* detail, uint32_t detail_cap);

#define USDK_RESOLUTION_MAX 64

/* The full, ordered load plan for `requested_names` (load-before
 * relationships preserved - a dependency always appears before its
 * dependent), computed by a real topological sort (Kahn's algorithm)
 * over the COMPLETE transitive dependency graph reachable from the
 * requested set within `set` - never a shortest-path substitute (see
 * docs/ABI.md). Fails with USDK_ERR_DEPENDENCY_CYCLE if the graph has a
 * cycle, USDK_ERR_DEPENDENCY_UNRESOLVED if a named dependency has no
 * matching manifest in `set`, or
 * USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE if a matching manifest exists
 * but its version is below the depending manifest's declared minimum. */
typedef struct usdk_resolution_plan {
    const usdk_manifest_t* order[USDK_RESOLUTION_MAX];
    uint32_t                count;
} usdk_resolution_plan_t;

USDK_EXPORT usdk_status_t USDK_CALL usdk_manifest_resolve(
    const usdk_manifest_set_t* set,
    const char* const* requested_names, uint32_t requested_count,
    usdk_resolution_plan_t* out_plan,
    char* detail, uint32_t detail_cap);

USDK_END_DECLS

#endif /* USDK_MANIFEST_H */
