#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// HOOKING: INITIALIZATION
// ============================================================================

int memkit_hook_init(int mode, bool debuggable) {
    // Initialize ShadowHook with specified mode
    // SHADOWHOOK_MODE_UNIQUE: Same address can only be hooked once (recommended)
    // SHADOWHOOK_MODE_SHARED: Same address can be hooked multiple times (with recursion prevention)
    // SHADOWHOOK_MODE_MULTI: Same address can be hooked multiple times (no prevention)
    int ret = shadowhook_init((shadowhook_mode_t)mode, debuggable);
    
    // Propagate error to errno for consistent error handling
    if (ret != 0) {
        errno = ret;  // ShadowHook returns positive error codes
    }
    
    return ret;
}

// ============================================================================
// HOOKING: HOOK BY ADDRESS (FUNCTION)
// FIXED: Returns stub handle for later unhooking
// FIXED: Uses out parameter for original function
// ============================================================================

void* memkit_hook(uintptr_t target_addr, void* replace_func, void** out_orig_func) {
    // Validate inputs
    if (target_addr == 0 || !replace_func) {
        errno = EINVAL;
        return NULL;
    }

    void* original_func = NULL;

    // Hook the function
    void* stub = shadowhook_hook_func_addr(
        (void*)target_addr,   // target function address
        replace_func,         // our replacement function (proxy)
        &original_func        // output: original function pointer
    );

    if (!stub) {
        // Propagate ShadowHook error to errno
        errno = shadowhook_get_errno();
        return NULL;
    }

    // Store original function if output pointer provided
    if (out_orig_func) {
        *out_orig_func = original_func;
    }

    // Return stub handle (user needs this for unhook)
    return stub;
}

// ============================================================================
// HOOKING: UNHOOK
// ============================================================================

void memkit_unhook(void* stub) {
    if (stub) {
        shadowhook_unhook(stub);
    }
}

// ============================================================================
// HOOKING: HOOK BY SYMBOL NAME
// ============================================================================

void* memkit_hook_by_symbol(const char* lib_name, const char* symbol_name, void* replace_func, void** out_orig_func) {
    // Validate inputs
    if (!symbol_name || !replace_func) {
        errno = EINVAL;
        return NULL;
    }
    
    // lib_name is optional (can hook from any loaded library)
    
    void* original_func = NULL;

    // Hook by symbol name
    void* stub = shadowhook_hook_sym_name(
        lib_name,           // target library (e.g., "libil2cpp.so") - optional
        symbol_name,        // symbol to hook (e.g., "il2cpp_thread_attach")
        replace_func,       // our replacement function (proxy)
        &original_func      // output: original function pointer
    );

    if (!stub) {
        // Propagate ShadowHook error to errno
        errno = shadowhook_get_errno();
        return NULL;
    }

    // Store original function if output pointer provided
    if (out_orig_func) {
        *out_orig_func = original_func;
    }

    return stub;
}

// ============================================================================
// HOOKING: ERROR HANDLING
// ============================================================================

int memkit_errno(void) {
    return shadowhook_get_errno();
}

const char *memkit_strerror(int errno_code) {
    return shadowhook_to_errmsg(errno_code);
}

const char *memkit_version(void) {
    return shadowhook_get_version();
}

int memkit_init_errno(void) {
    return shadowhook_get_init_errno();
}

// ============================================================================
// HOOKING: WITH CALLBACK
// ============================================================================

void *memkit_hook_with_callback(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, MemKitHooked hooked, void *hooked_arg) {
    if (!lib_name || !sym_name || !new_addr) return NULL;

    void *stub = shadowhook_hook_sym_name_callback(
        lib_name,
        sym_name,
        new_addr,
        orig_addr,
        (shadowhook_hooked_t)hooked,
        hooked_arg
    );

    if (!stub && orig_addr) *orig_addr = NULL;
    return stub;
}

void *memkit_hook_by_symbol_callback(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, MemKitHooked hooked, void *hooked_arg) {
    return memkit_hook_with_callback(lib_name, sym_name, new_addr, orig_addr, hooked, hooked_arg);
}
