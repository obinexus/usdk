#if defined(_WIN32)
#include "platform_load.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct usdk_native_lib { HMODULE handle; };

usdk_native_lib_t* usdk_platform_load(const char* path, char* err, unsigned err_cap) {
    /* LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: searches the loading module's own
     * directory, the application directory, and system directories -
     * never PATH or the current directory. See docs/ABI.md "Platform
     * loading" for why this matters (every USDK artifact lands in one
     * output directory so this resolves sibling modules correctly). */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0) {
        if (err && err_cap) snprintf(err, err_cap, "invalid UTF-8 path");
        return NULL;
    }
    WCHAR* wpath = (WCHAR*)malloc((size_t)wlen * sizeof(WCHAR));
    if (!wpath) {
        if (err && err_cap) snprintf(err, err_cap, "out of memory");
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);

    HMODULE h = LoadLibraryExW(wpath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    free(wpath);
    if (!h) {
        if (err && err_cap) snprintf(err, err_cap, "LoadLibraryExW failed (GetLastError=%lu)", GetLastError());
        return NULL;
    }
    usdk_native_lib_t* lib = (usdk_native_lib_t*)malloc(sizeof(*lib));
    if (!lib) { FreeLibrary(h); if (err && err_cap) snprintf(err, err_cap, "out of memory"); return NULL; }
    lib->handle = h;
    return lib;
}

void* usdk_platform_symbol(usdk_native_lib_t* lib, const char* name) {
    if (!lib) return NULL;
    return (void*)GetProcAddress(lib->handle, name);
}

void usdk_platform_unload(usdk_native_lib_t* lib) {
    if (!lib) return;
    FreeLibrary(lib->handle);
    free(lib);
}
#endif /* _WIN32 */
