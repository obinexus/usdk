#ifndef USDK_PLUGIN_H
#define USDK_PLUGIN_H

#include "usdk/types.h"
#include "usdk/status.h"
#include "usdk/candidate.h"
#include "usdk/vote.h"

USDK_BEGIN_DECLS

#define USDK_ABI_VERSION_MAJOR 1u
#define USDK_ABI_VERSION_MINOR 0u

/* Exported symbol name every loadable module must define, matching
 * usdk_plugin_query_v1_fn below. See docs/ABI.md "Entry point". */
#define USDK_PLUGIN_QUERY_SYMBOL "usdk_plugin_query_v1"

/* Checked, in order, by usdk-ffi before a single field of the vtable a
 * module returns is touched - see docs/ABI.md "ABI version and
 * struct-size negotiation". */
typedef struct usdk_descriptor {
    uint32_t    struct_size; /* sizeof(usdk_descriptor_t) as the MODULE
                               * was built - checked first, against the
                               * caller's own sizeof, before anything
                               * else in this struct is read. */
    uint32_t    abi_version_major;
    uint32_t    abi_version_minor;
    usdk_role_t role;
    char        party_id[USDK_ID_LEN];
    uint32_t    capability_flags; /* USDK_CAP_* below, bitwise OR */
} usdk_descriptor_t;

#define USDK_CAP_VOTE    0x1u /* implements vote() - required for
                               * perceive/deliberate/verify */
#define USDK_CAP_PROPOSE 0x2u /* implements propose() - deliberate only */
#define USDK_CAP_DRIVER  0x4u /* implements usdk_driver_vtable_t's
                               * generate() instead of the role vtable -
                               * mutually exclusive with the two above */

typedef struct usdk_role_instance usdk_role_instance_t; /* opaque */

/* Opaque, module-defined configuration payload - each role/driver parses
 * its own `data` (typically small JSON) and documents its own schema in
 * its header; usdk-core and usdk-ffi never interpret it. */
typedef struct usdk_config {
    usdk_buffer_t data;
} usdk_config_t;

typedef usdk_status_t (USDK_CALL *usdk_role_create_fn)(
    const usdk_config_t* cfg, usdk_role_instance_t** out);
typedef void (USDK_CALL *usdk_role_destroy_fn)(usdk_role_instance_t* inst);

/* Casts a verdict on `candidate`. Required (CAP_VOTE) for all three
 * constituent roles - see docs/ARCHITECTURE.md for the distinct check
 * each role's implementation of this function actually performs. Not
 * thread-safe: a caller must not call this concurrently on the same
 * `inst` from multiple threads (docs/ABI.md "Threading and callback
 * rules"). */
typedef usdk_status_t (USDK_CALL *usdk_role_vote_fn)(
    usdk_role_instance_t* inst,
    const usdk_candidate_t* candidate,
    usdk_vote_t* out_vote);

/* usdk-deliberate only (CAP_PROPOSE): produces the proposed_response
 * payload for a new candidate from the supplied evidence, using
 * whichever driver this deliberate instance was configured with. The
 * returned buffer is owned by the caller, released with
 * usdk_owned_buffer_release (usdk/core.h). */
typedef usdk_status_t (USDK_CALL *usdk_role_propose_fn)(
    usdk_role_instance_t* inst,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t prompt,
    usdk_owned_buffer_t* out_response);

typedef struct usdk_role_vtable {
    uint32_t             struct_size;
    usdk_role_create_fn  create;
    usdk_role_destroy_fn destroy;
    usdk_role_vote_fn    vote;    /* NULL only if CAP_VOTE not set - never
                                    * the case for perceive/deliberate/verify */
    usdk_role_propose_fn propose; /* NULL unless CAP_PROPOSE is set */
} usdk_role_vtable_t;

/* usdk-driver-<backend> modules (CAP_DRIVER) implement this instead of
 * usdk_role_vtable_t - loaded and called only by usdk-deliberate (never
 * directly by usdk-core), via usdk-ffi the same way any module is
 * loaded. `prompt` and the evidence are borrowed; `out_response` is
 * caller-owned, released with usdk_owned_buffer_release. */
typedef usdk_status_t (USDK_CALL *usdk_driver_generate_fn)(
    usdk_role_instance_t* inst,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t prompt,
    usdk_owned_buffer_t* out_response);

typedef struct usdk_driver_vtable {
    uint32_t                struct_size;
    usdk_role_create_fn     create;
    usdk_role_destroy_fn    destroy;
    usdk_driver_generate_fn generate;
} usdk_driver_vtable_t;

/* The one exported entry point every loadable module defines. `*out_vtable`
 * is `const usdk_role_vtable_t*` when `(*out_descriptor)->role` is
 * PERCEIVE/DELIBERATE/VERIFY, or `const usdk_driver_vtable_t*` when it is
 * USDK_ROLE_DRIVER - the caller selects which to cast to based on `role`,
 * which it already knows from what capability it asked usdk-ffi to
 * resolve. See docs/ABI.md "Entry point" for what this function does and
 * does not guarantee. */
typedef usdk_status_t (USDK_CALL *usdk_plugin_query_v1_fn)(
    const usdk_descriptor_t** out_descriptor,
    const void** out_vtable);

USDK_END_DECLS

#endif /* USDK_PLUGIN_H */
