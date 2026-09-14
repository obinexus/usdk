#include "../unit/usdk_test.h"
#include "usdk/ffi.h"

#ifndef USDK_TEST_BAD_ABI_MODULE_PATH
#error "USDK_TEST_BAD_ABI_MODULE_PATH must be defined by the build"
#endif

USDK_TEST_MAIN_BEGIN()
    usdk_module_t* mod = NULL;
    const usdk_descriptor_t* desc = NULL;
    const void* vt = NULL;
    usdk_status_t st = usdk_ffi_load(USDK_TEST_BAD_ABI_MODULE_PATH, &mod, &desc, &vt);
    USDK_CHECK_EQ_INT(st, USDK_ERR_ABI_VERSION_MISMATCH);
    /* Must not leave a module handle behind on a rejected load. */
    USDK_CHECK(mod == NULL);
USDK_TEST_MAIN_END()
