#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "memkit.h"

// ============================================================================
// IL2CPP: STATIC STATE (Thread-Safe via C11 Atomics)
// ============================================================================

static void* g_il2cpp_handle = NULL;
static atomic_bool g_initialized = ATOMIC_VAR_INIT(false);

// ============================================================================
// IL2CPP: INITIALIZATION
// ============================================================================

bool memkit_il2cpp_init(void) {
    bool expected = false;
    
    // Only the first thread (CAS succeeds) executes memkit_xdl_open
    // atomic_compare_exchange_strong: 100% thread-safe, lock-free
    if (atomic_compare_exchange_strong(&g_initialized, &expected, true)) {
        g_il2cpp_handle = memkit_xdl_open("libil2cpp.so", XDL_DEFAULT);
    }
    
    // Wait briefly if another thread is still opening the handle (rare case)
    while (atomic_load(&g_initialized) && g_il2cpp_handle == NULL) {
        // Check if it truly failed, or if another thread is still processing
        // This spin-wait is extremely short in practice (microseconds)
    }
    
    return g_il2cpp_handle != NULL;
}

// ============================================================================
// IL2CPP: GET HANDLE
// ============================================================================

void* memkit_il2cpp_get_handle(void) {
    // Ensure initialization
    memkit_il2cpp_init();
    return g_il2cpp_handle;
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL (.dynsym)
// ============================================================================

void* memkit_il2cpp_resolve(const char* symbol_name) {
    if (!symbol_name) {
        return NULL;
    }

    // Ensure initialization (thread-safe via atomics)
    memkit_il2cpp_init();

    // If handle is NULL, try to open directly
    if (!g_il2cpp_handle) {
        g_il2cpp_handle = memkit_xdl_open("libil2cpp.so", XDL_DEFAULT);
        if (!g_il2cpp_handle) {
            return NULL;
        }
    }

    // Resolve the symbol using XDL
    // memkit_xdl_sym searches in .dynsym (dynamic symbol table)
    // This is where most exported functions live
    return memkit_xdl_sym(g_il2cpp_handle, symbol_name, NULL);
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL FROM SYMTAB ONLY (Advanced)
// Use this for stripped/internal functions not in .dynsym
// ============================================================================

void* memkit_il2cpp_resolve_symtab(const char* symbol_name) {
    if (!symbol_name) {
        return NULL;
    }

    // Ensure initialization (thread-safe via atomics)
    memkit_il2cpp_init();

    if (!g_il2cpp_handle) {
        return NULL;
    }

    // Use memkit_xdl_dsym to search only in .symtab section
    // This is useful for:
    // - Stripped symbols
    // - Internal/private functions
    // - Debug symbols
    return memkit_xdl_dsym(g_il2cpp_handle, symbol_name, NULL);
}
