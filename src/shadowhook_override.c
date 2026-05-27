#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "nothing_path.h"

// ============================================================================
// LINKER WRAP INTERFACES
//
// These functions are linked via --wrap flags in LDFLAGS:
//   -Wl,--wrap,dlopen        → __wrap_dlopen / __real_dlopen
//   -Wl,--wrap,sh_linker_init → __wrap_sh_linker_init / __real_sh_linker_init
//
// The real implementations are from:
//   dlopen()        → libc (bionic's dynamic linker)
//   sh_linker_init() → ShadowHook's statically-linked code
// ============================================================================

extern void *__real_dlopen(const char *filename, int flags);
extern int __real_sh_linker_init(void);

// Forward declarations for --wrap entry points
// These are called by the linker when --wrap,dlopen and --wrap,sh_linker_init
// are used. Declared here to suppress -Wmissing-prototypes.
void *__wrap_dlopen(const char *filename, int flags);
int __wrap_sh_linker_init(void);

// ============================================================================
// Helpers
// ============================================================================

/**
 * Check if filename refers to libshadowhook_nothing.so (by basename).
 *
 * This is more precise than strstr() which could match paths like
 * "/some/libfake_libshadowhook_nothing.so_copy".
 */
static bool is_nothing_lib(const char *filename) {
    if (!filename) return false;
    const char *base = strrchr(filename, '/');
    if (!base) {
        base = filename;
    } else {
        base++;
    }
    return strcmp(base, "libshadowhook_nothing.so") == 0;
}

// ============================================================================
// __wrap_dlopen — Intercept dlopen calls for libshadowhook_nothing.so
//
// ShadowHook's sh_linker_soinfo_memory_scan_pre (called during sh_linker_init)
// attempts dlopen("libshadowhook_nothing.so") to get the linker to create a
// soinfo entry. On Android 15+ with non-standard injection paths, this fails
// because the library isn't in the app's linker namespace.
//
// We intercept the call and redirect it to either:
//   1. A user-specified path (via memkit_set_nothing_path())
//   2. An auto-extracted temp file from the embedded blob
// ============================================================================

void *__wrap_dlopen(const char *filename, int flags) {
    if (is_nothing_lib(filename)) {
        char *path = memkit_ensure_nothing_path();
        if (path) {
            void *handle = __real_dlopen(path, flags);
            if (handle) {
                memkit_consume_nothing_path();
            }
            free(path);
            return handle;
        }
        __android_log_print(ANDROID_LOG_WARN, "memkit",
            "nothing path resolution failed, falling through to real dlopen. "
            "ShadowHook linker init will likely fail.");
    }
    return __real_dlopen(filename, flags);
}

// ============================================================================
// __wrap_sh_linker_init — Intercept ShadowHook linker initialization
//
// Strategy:
//   1. Try the real sh_linker_init() first. Our __wrap_dlopen ensures the
//      nothing library can be found, which may allow linker init to succeed
//      on some devices/configurations.
//   2. If the real init fails (e.g., xdl_open still uses a hardcoded path
//      that doesn't exist), fall back to skipping linker init entirely.
//
// When linker init is skipped:
//   - Core hooking APIs work fine:
//       shadowhook_hook_func_addr()   ✅
//       shadowhook_hook_sym_addr()    ✅
//       shadowhook_hook_sym_name()    ✅
//       shadowhook_hook_sym_name_callback() ✅
//   - dl_init/dl_fini callbacks are disabled:
//       shadowhook_register_dl_init_callback() ❌
//       shadowhook_register_dl_fini_callback() ❌
// ============================================================================

int __wrap_sh_linker_init(void) {
    // Try real init first — our __wrap_dlopen makes the nothing library findable
    int ret = __real_sh_linker_init();
    if (ret == 0) return 0;

    // Fallback: skip linker module init
    __android_log_print(ANDROID_LOG_WARN, "memkit",
        "sh_linker_init failed (%d): dl_init/dl_fini callbacks disabled. "
        "Core hooking works.", ret);
    return 0;
}
