#if !defined(_WIN32)
#include "platform_load.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct usdk_native_lib { void* handle; };

usdk_native_lib_t* usdk_platform_load(const char* path, char* err, unsigned err_cap) {
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        if (err && err_cap) snprintf(err, err_cap, "dlopen failed: %s", dlerror());
        return NULL;
    }
    usdk_native_lib_t* lib = (usdk_native_lib_t*)malloc(sizeof(*lib));
    if (!lib) { dlclose(h); if (err && err_cap) snprintf(err, err_cap, "out of memory"); return NULL; }
    lib->handle = h;
    return lib;
}

void* usdk_platform_symbol(usdk_native_lib_t* lib, const char* name) {
    if (!lib) return NULL;
    return dlsym(lib->handle, name);
}

void usdk_platform_unload(usdk_native_lib_t* lib) {
    if (!lib) return;
    dlclose(lib->handle);
    free(lib);
}
#endif /* !_WIN32 */
