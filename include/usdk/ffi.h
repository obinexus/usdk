#ifndef USDK_FFI_H
#define USDK_FFI_H

#include "usdk/types.h"
#include "usdk/status.h"
#include "usdk/plugin.h"
#include "usdk/manifest.h"

USDK_BEGIN_DECLS

typedef struct usdk_module usdk_module_t; /* opaque - one loaded shared library */

/* Loads `path` (LoadLibraryExW on Windows with
 * LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, dlopen(RTLD_NOW|RTLD_LOCAL) on Unix -
 * see docs/ABI.md "Platform loading"), resolves USDK_PLUGIN_QUERY_SYMBOL,
 * calls it, and validates the returned descriptor's struct_size and ABI
 * version before returning success - see docs/ABI.md "ABI version and
 * struct-size negotiation". On success, `*out_descriptor`/`*out_vtable`
 * point into memory owned by the module (valid until usdk_ffi_unload) -
 * they are not copied. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_ffi_load(
    const char* path,
    usdk_module_t** out_module,
    const usdk_descriptor_t** out_descriptor,
    const void** out_vtable);

/* Returns USDK_ERR_INVALID_ARGUMENT (and does not unload) if `module`
 * still has outstanding role instances created from it - see docs/ABI.md
 * "Keeping modules loaded, and safe shutdown". */
USDK_EXPORT usdk_status_t USDK_CALL usdk_ffi_unload(usdk_module_t* module);

/* Called by usdk_role_create/usdk_driver's create() wrapper bookkeeping
 * (src/ffi/ffi.c) to track outstanding instances against their owning
 * module - not intended to be called directly by ordinary users of this
 * header; exposed because usdk-core's instance-owning helpers need it. */
USDK_EXPORT void USDK_CALL usdk_ffi_instance_created(usdk_module_t* module);
USDK_EXPORT void USDK_CALL usdk_ffi_instance_destroyed(usdk_module_t* module);

/* Resolves and loads every module needed to satisfy `requested_names`
 * from the manifests in `manifest_dir`, in dependency order (see
 * usdk_manifest_resolve). On success, `out_modules`/`out_descriptors`/
 * `out_vtables` are parallel arrays of length `*out_count`
 * (capacity `USDK_RESOLUTION_MAX`), already-loaded and ABI-validated. On
 * any failure, every module already loaded during this call is unloaded
 * before returning, so a failed usdk_ffi_load_resolved never leaves
 * partially-loaded state behind. */
USDK_EXPORT usdk_status_t USDK_CALL usdk_ffi_load_resolved(
    const char* manifest_dir,
    const char* const* requested_names, uint32_t requested_count,
    usdk_module_t** out_modules,
    const usdk_descriptor_t** out_descriptors,
    const void** out_vtables,
    uint32_t* out_count,
    char* detail, uint32_t detail_cap);

USDK_END_DECLS

#endif /* USDK_FFI_H */
