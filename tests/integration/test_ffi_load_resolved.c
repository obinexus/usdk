/* Loads the REAL usdk-perceive/usdk-deliberate/usdk-verify modules (plus
 * usdk-deliberate's transitively-declared usdk-driver-fixture dependency)
 * from the project's actual manifests/ directory, through the full
 * usdk-ffi dynamic-loading + manifest-resolution path - complementing
 * tests/unit/test_manifest.c, which only exercises resolution logic
 * in-memory. */

#include "../unit/usdk_test.h"
#include "usdk/ffi.h"

#ifndef USDK_TEST_MANIFEST_DIR
#error "USDK_TEST_MANIFEST_DIR must be defined by the build"
#endif

USDK_TEST_MAIN_BEGIN()
    const char* names[3] = { "usdk-perceive", "usdk-deliberate", "usdk-verify" };
    usdk_module_t* modules[8];
    const usdk_descriptor_t* descs[8];
    const void* vts[8];
    uint32_t count = 0;
    char detail[256] = {0};

    usdk_status_t st = usdk_ffi_load_resolved(USDK_TEST_MANIFEST_DIR, names, 3,
                                               modules, descs, vts, &count, detail, sizeof(detail));
    USDK_CHECK_EQ_INT(st, USDK_OK);
    /* perceive, deliberate, verify, and the transitively-pulled fixture
     * driver - 4 modules from a 3-name request. */
    USDK_CHECK_EQ_INT(count, 4);

    int saw_perceive = 0, saw_deliberate = 0, saw_verify = 0, saw_driver = 0;
    for (uint32_t i = 0; i < count; ++i) {
        USDK_CHECK_EQ_INT(descs[i]->abi_version_major, USDK_ABI_VERSION_MAJOR);
        if (descs[i]->role == USDK_ROLE_PERCEIVE) saw_perceive = 1;
        if (descs[i]->role == USDK_ROLE_DELIBERATE) saw_deliberate = 1;
        if (descs[i]->role == USDK_ROLE_VERIFY) saw_verify = 1;
        if (descs[i]->role == USDK_ROLE_DRIVER) saw_driver = 1;
    }
    USDK_CHECK(saw_perceive && saw_deliberate && saw_verify && saw_driver);

    for (uint32_t i = 0; i < count; ++i) usdk_ffi_unload(modules[i]);
USDK_TEST_MAIN_END()
