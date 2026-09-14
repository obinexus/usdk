#ifndef USDK_PLATFORM_LOAD_H
#define USDK_PLATFORM_LOAD_H

/* Internal-only interface hiding LoadLibraryExW/GetProcAddress (Windows)
 * vs dlopen/dlsym (Unix) behind one shape - see docs/ABI.md "Platform
 * loading". Not installed, not part of the public ABI. */

typedef struct usdk_native_lib usdk_native_lib_t; /* opaque */

/* Returns NULL on failure; `err`/`err_cap` receive a short platform
 * error description (always NUL-terminated if err_cap > 0). */
usdk_native_lib_t* usdk_platform_load(const char* path, char* err, unsigned err_cap);

void* usdk_platform_symbol(usdk_native_lib_t* lib, const char* name);

void usdk_platform_unload(usdk_native_lib_t* lib);

#endif /* USDK_PLATFORM_LOAD_H */
