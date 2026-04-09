#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// DL INIT CALLBACKS
// ============================================================================

int memkit_register_dl_init_callback(MemKitDlInitCallback pre, MemKitDlInitCallback post, void *data) {
    if (!pre && !post) return MK_ERRNO_INVALID_ARG;
    return shadowhook_register_dl_init_callback(
        (shadowhook_dl_info_t)pre,
        (shadowhook_dl_info_t)post,
        data
    );
}

int memkit_unregister_dl_init_callback(MemKitDlInitCallback pre, MemKitDlInitCallback post, void *data) {
    if (!pre && !post) return MK_ERRNO_INVALID_ARG;
    return shadowhook_unregister_dl_init_callback(
        (shadowhook_dl_info_t)pre,
        (shadowhook_dl_info_t)post,
        data
    );
}

// ============================================================================
// DL FINI CALLBACKS
// ============================================================================

int memkit_register_dl_fini_callback(MemKitDlFiniCallback pre, MemKitDlFiniCallback post, void *data) {
    if (!pre && !post) return MK_ERRNO_INVALID_ARG;
    return shadowhook_register_dl_fini_callback(
        (shadowhook_dl_info_t)pre,
        (shadowhook_dl_info_t)post,
        data
    );
}

int memkit_unregister_dl_fini_callback(MemKitDlFiniCallback pre, MemKitDlFiniCallback post, void *data) {
    if (!pre && !post) return MK_ERRNO_INVALID_ARG;
    return shadowhook_unregister_dl_fini_callback(
        (shadowhook_dl_info_t)pre,
        (shadowhook_dl_info_t)post,
        data
    );
}
