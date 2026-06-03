#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <dlfcn.h>
#include <link.h>
#include <sched.h>

#include "memkit.h"

// ============================================================================
// IL2CPP: STATIC STATE (Thread-Safe via C11 Atomics)
// ============================================================================

static _Atomic void* g_il2cpp_handle = NULL;
static atomic_bool g_initialized;
static atomic_bool g_init_failed;

// dl_iterate_phdr callback: find libil2cpp.so and create an XDL handle
// from its dl_phdr_info, bypassing linker namespace restrictions.
//
// NOTE: dl_iterate_phdr holds a linker lock internally, so this callback
// must not trigger any operation that could recurse back into dl_iterate_phdr
// (e.g., dlopen, xdl_open). memkit_xdl_open_from_phdr() is safe because it
// only copies phdr data — it does not call dlopen or dl_iterate_phdr.
static int find_libil2cpp(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    if (info->dlpi_name && strstr(info->dlpi_name, "libil2cpp.so")) {
        void **handle_out = (void **)data;
        *handle_out = memkit_xdl_open_from_phdr(info);
        return (*handle_out != NULL) ? 1 : 0;
    }
    return 0;
}

// Fallback when memkit_xdl_open("libil2cpp.so") fails due to linker
// namespace restrictions (Android 15+). Uses dl_iterate_phdr to find
// the already-loaded library and creates a handle from its phdr info.
static void* try_open_libil2cpp_fallback(void) {
    void *handle = NULL;
    dl_iterate_phdr(find_libil2cpp, &handle);
    if (!handle) {
        __android_log_print(ANDROID_LOG_WARN, "memkit",
            "il2cpp: dl_iterate_phdr fallback did not find libil2cpp.so");
    }
    return handle;
}

/// Check if another thread has claimed init but hasn't set the handle yet.
/// Used in the spin-wait loop to yield until initialization completes or fails.
///
/// NOTE: Loads g_initialized and g_il2cpp_handle as separate atomic operations
/// (not a single atomic pair). This is intentional — the two values are never
/// updated simultaneously by the init thread (handle is stored after init flag),
/// so a benign transient state may exist where is_init_pending() returns true
/// while g_init_failed is also set, causing at most one extra spin iteration.
/// No correctness or infinite-loop risk.
static inline bool is_init_pending(void) {
    return atomic_load_explicit(&g_initialized, memory_order_acquire)
        && atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire) == NULL;
}

/// Ensure the XDL handle is available. If init_internal failed but the
/// library is now loadable (e.g., linker namespace became accessible),
/// attempts a fallback open via dl_iterate_phdr. Thread-safe.
/// Returns true if handle is available, false otherwise.
static inline bool ensure_handle(void) {
    if (atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire))
        return true;
    void *h = try_open_libil2cpp_fallback();
    void *expected = NULL;
    if (!atomic_compare_exchange_strong(&g_il2cpp_handle, &expected, h)) {
        if (h) memkit_xdl_close(h);
    }
    return atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire) != NULL;
}

/// Wait for the init thread to complete initialization.
/// Uses bounded spin with backoff: yield/pause (0-99), sched_yield (100-999),
/// then gives up (1000+).
/// Returns true if handle is available, false if init failed or timed out.
static bool wait_for_init_completion(void) {
    int spin_count = 0;
    while (is_init_pending() && !atomic_load_explicit(&g_init_failed, memory_order_acquire)) {
        if (++spin_count < 100) {
#if defined(__aarch64__) || defined(__arm__)
            __asm__ volatile("yield");
#elif defined(__i386__) || defined(__x86_64__)
            __asm__ volatile("pause");
#else
            /* fallback: full memory fence for non-x86/ARM architectures */
            atomic_thread_fence(memory_order_acq_rel);
#endif
        } else if (spin_count < 1000) {
            sched_yield();
        } else {
            break;  // give up after ~1000 iterations
        }
    }
    return atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire) != NULL;
}

// ============================================================================
// IL2CPP: INITIALIZATION (Internal)
// ============================================================================

static bool memkit_il2cpp_init_internal(void) {
    bool expected = false;

    // Only the first thread (CAS succeeds) executes initialization
    if (atomic_compare_exchange_strong(&g_initialized, &expected, true)) {
        // Primary: xdl_open — works for most cases.
        // Fallback: dl_iterate_phdr — handles Android 15+ namespace restrictions.
        void *h = memkit_xdl_open("libil2cpp.so", XDL_DEFAULT);
        atomic_store_explicit(&g_il2cpp_handle, h, memory_order_release);

        if (!atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire)) {
            void *h2 = try_open_libil2cpp_fallback();
            atomic_store_explicit(&g_il2cpp_handle, h2, memory_order_release);
        }

        // If all attempts failed, mark init as failed so waiting
        // threads can bail out instead of spinning forever
        if (!atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire)) {
            __android_log_print(ANDROID_LOG_ERROR, "memkit",
                "il2cpp: xdl_open and dl_iterate_phdr fallback both failed");
            atomic_store_explicit(&g_init_failed, true, memory_order_release);
        }
    }

    return wait_for_init_completion();
}

// ============================================================================
// IL2CPP: GET HANDLE
// ============================================================================

void* memkit_il2cpp_get_handle(void) {
    memkit_il2cpp_init_internal();
    ensure_handle();
    return atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire);
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL INTERNAL (Shared Prologue)
// ============================================================================

static inline void *resolve_impl(const char *symbol_name, bool use_symtab) {
    if (!symbol_name || symbol_name[0] == '\0') return NULL;
    memkit_il2cpp_init_internal();
    if (!ensure_handle()) return NULL;
    void *handle = atomic_load_explicit(&g_il2cpp_handle, memory_order_acquire);
    if (use_symtab)
        return memkit_xdl_dsym(handle, symbol_name, NULL);
    return memkit_xdl_sym(handle, symbol_name, NULL);
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL (.dynsym)
// ============================================================================

void* memkit_il2cpp_resolve(const char *symbol_name) {
    return resolve_impl(symbol_name, false);
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL FROM SYMTAB ONLY (Advanced)
// Use this for stripped/internal functions not in .dynsym
// ============================================================================

void* memkit_il2cpp_resolve_symtab(const char *symbol_name) {
    return resolve_impl(symbol_name, true);
}
