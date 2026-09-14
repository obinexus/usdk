#include "usdk/ffi.h"
#include "platform_load.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct usdk_module {
    usdk_native_lib_t* lib;
    const usdk_descriptor_t* descriptor;
    const void* vtable;
    uint32_t    instance_count; /* see docs/ABI.md "Keeping modules loaded, and safe shutdown" */
};

usdk_status_t usdk_ffi_load(
    const char* path,
    usdk_module_t** out_module,
    const usdk_descriptor_t** out_descriptor,
    const void** out_vtable) {
    if (!path || !out_module) return USDK_ERR_INVALID_ARGUMENT;

    char err[256];
    usdk_native_lib_t* lib = usdk_platform_load(path, err, (unsigned)sizeof(err));
    if (!lib) return USDK_ERR_MODULE_LOAD_FAILED;

    usdk_plugin_query_v1_fn query =
        (usdk_plugin_query_v1_fn)usdk_platform_symbol(lib, USDK_PLUGIN_QUERY_SYMBOL);
    if (!query) {
        usdk_platform_unload(lib);
        return USDK_ERR_MODULE_ENTRY_NOT_FOUND;
    }

    const usdk_descriptor_t* descriptor = NULL;
    const void* vtable = NULL;
    usdk_status_t qst = query(&descriptor, &vtable);
    if (qst != USDK_OK || !descriptor || !vtable) {
        usdk_platform_unload(lib);
        return USDK_ERR_MODULE_LOAD_FAILED;
    }

    /* See docs/ABI.md "ABI version and struct-size negotiation": check
     * struct_size before trusting any other field, then the ABI version,
     * before this module's vtable is ever called into. */
    if (descriptor->struct_size < (uint32_t)sizeof(usdk_descriptor_t)) {
        usdk_platform_unload(lib);
        return USDK_ERR_STRUCT_SIZE_MISMATCH;
    }
    if (descriptor->abi_version_major != USDK_ABI_VERSION_MAJOR ||
        descriptor->abi_version_minor < USDK_ABI_VERSION_MINOR) {
        usdk_platform_unload(lib);
        return USDK_ERR_ABI_VERSION_MISMATCH;
    }

    usdk_module_t* mod = (usdk_module_t*)calloc(1, sizeof(*mod));
    if (!mod) { usdk_platform_unload(lib); return USDK_ERR_OUT_OF_MEMORY; }
    mod->lib = lib;
    mod->descriptor = descriptor;
    mod->vtable = vtable;
    mod->instance_count = 0;

    *out_module = mod;
    if (out_descriptor) *out_descriptor = descriptor;
    if (out_vtable) *out_vtable = vtable;
    return USDK_OK;
}

usdk_status_t usdk_ffi_unload(usdk_module_t* module) {
    if (!module) return USDK_ERR_INVALID_ARGUMENT;
    if (module->instance_count > 0) return USDK_ERR_INVALID_ARGUMENT;
    usdk_platform_unload(module->lib);
    free(module);
    return USDK_OK;
}

void usdk_ffi_instance_created(usdk_module_t* module) {
    if (module) module->instance_count++;
}

void usdk_ffi_instance_destroyed(usdk_module_t* module) {
    if (module && module->instance_count > 0) module->instance_count--;
}

usdk_status_t usdk_ffi_load_resolved(
    const char* manifest_dir,
    const char* const* requested_names, uint32_t requested_count,
    usdk_module_t** out_modules,
    const usdk_descriptor_t** out_descriptors,
    const void** out_vtables,
    uint32_t* out_count,
    char* detail, uint32_t detail_cap) {
    if (!manifest_dir || !requested_names || !out_modules || !out_count) return USDK_ERR_INVALID_ARGUMENT;

    usdk_manifest_set_t set;
    usdk_status_t st = usdk_manifest_load_dir(manifest_dir, &set, detail, detail_cap);
    if (st != USDK_OK) return st;

    usdk_resolution_plan_t plan;
    st = usdk_manifest_resolve(&set, requested_names, requested_count, &plan, detail, detail_cap);
    if (st != USDK_OK) return st;

#if defined(_WIN32)
    static const char* k_platform_ext = ".dll";
#elif defined(__APPLE__)
    static const char* k_platform_ext = ".dylib";
#else
    static const char* k_platform_ext = ".so";
#endif

    uint32_t loaded = 0;
    for (uint32_t i = 0; i < plan.count; ++i) {
        /* artifact_path may omit the platform's shared-library extension
         * (the common, portable case - one manifest works on every OS);
         * if it already has a known one, it is used verbatim. */
        const char* ap = plan.order[i]->artifact_path;
        size_t ap_len = strlen(ap);
        int has_known_ext =
            (ap_len >= 3 && strcmp(ap + ap_len - 3, ".so") == 0) ||
            (ap_len >= 4 && strcmp(ap + ap_len - 4, ".dll") == 0) ||
            (ap_len >= 6 && strcmp(ap + ap_len - 6, ".dylib") == 0);
        char full_path[USDK_MANIFEST_PATH_LEN + 32];
        snprintf(full_path, sizeof(full_path), "%s/%s%s", manifest_dir, ap,
                 has_known_ext ? "" : k_platform_ext);

        usdk_module_t* mod = NULL;
        const usdk_descriptor_t* desc = NULL;
        const void* vt = NULL;
        st = usdk_ffi_load(full_path, &mod, &desc, &vt);
        if (st != USDK_OK) {
            for (uint32_t k = 0; k < loaded; ++k) usdk_ffi_unload(out_modules[k]);
            if (detail && detail_cap) snprintf(detail, detail_cap, "failed to load %s", plan.order[i]->name);
            return st;
        }
        out_modules[loaded] = mod;
        if (out_descriptors) out_descriptors[loaded] = desc;
        if (out_vtables) out_vtables[loaded] = vt;
        loaded++;
    }
    *out_count = loaded;
    return USDK_OK;
}
