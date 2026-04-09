#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// HOOKING V2: BY SYMBOL NAME WITH FLAGS
// Note: shadowhook_hook_sym_name_2 does NOT take variadic record params
// in the current ShadowHook API — record params are only for _2 variants
// of hook_func_addr, hook_sym_addr, and intercept functions.
// ============================================================================

void *memkit_hook_v2(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, uint32_t flags) {
    if (!lib_name || !sym_name || !new_addr) return NULL;

    void *stub = shadowhook_hook_sym_name_2(lib_name, sym_name, new_addr, orig_addr, flags);

    if (!stub && orig_addr) *orig_addr = NULL;
    return stub;
}

void *memkit_hook_by_symbol_v2(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, uint32_t flags) {
    /* Same implementation as memkit_hook_v2 — ShadowHook's _2 API has same signature for sym_name */
    return memkit_hook_v2(lib_name, sym_name, new_addr, orig_addr, flags);
}
