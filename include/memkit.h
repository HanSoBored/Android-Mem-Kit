#ifndef MEMKIT_H
#define MEMKIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <link.h>

#include "shadowhook.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MEMORY PATCHING API
// ============================================================================

typedef struct {
    uintptr_t address;
    size_t size;
    uint8_t* orig_bytes;
    uint8_t* patch_bytes;
} MemPatch;

/**
 * Get the lowest base address of a loaded library
 * @param lib_name Name of the library (e.g., "libil2cpp.so")
 * @return Base address or 0 if not found
 */
uintptr_t memkit_get_lib_base(const char* lib_name);

/**
 * Create a memory patch from hex string
 * Automatically backs up original bytes safely (handles XOM)
 * @param address Target absolute address
 * @param hex_string Hex string with spaces (e.g., "00 00 A0 E3")
 * @return MemPatch pointer or NULL on failure
 */
MemPatch* memkit_patch_create(uintptr_t address, const char* hex_string);

/**
 * Apply the memory patch
 * @param patch MemPatch pointer
 * @return true on success, false on failure
 */
bool memkit_patch_apply(MemPatch* patch);

/**
 * Restore original bytes
 * @param patch MemPatch pointer
 * @return true on success, false on failure
 */
bool memkit_patch_restore(MemPatch* patch);

/**
 * Free MemPatch resources
 * @param patch MemPatch pointer
 */
void memkit_patch_free(MemPatch* patch);

// ============================================================================
// HOOKING API (ShadowHook)
// ============================================================================

/**
 * Initialize ShadowHook (call once at startup)
 * @param mode SHADOWHOOK_MODE_UNIQUE, SHARED, or MULTI
 * @param debuggable Enable debug logging
 * @return 0 on success, negative value on failure
 */
int memkit_hook_init(int mode, bool debuggable);

/**
 * Hook a function at target address
 * FIXED: Returns stub handle for later unhooking
 * FIXED: Uses out parameter for original function
 * 
 * @param target_addr Address of target function
 * @param replace_func Pointer to replacement function (proxy)
 * @param out_orig_func Output: pointer to original function
 * @return Stub handle (for unhook) or NULL on failure
 */
void* memkit_hook(uintptr_t target_addr, void* replace_func, void** out_orig_func);

/**
 * Unhook a function using stub handle
 * @param stub Handle returned by memkit_hook
 */
void memkit_unhook(void* stub);

/**
 * Hook by symbol name (convenience wrapper)
 * @param lib_name Library name (e.g., "libil2cpp.so")
 * @param symbol_name Symbol to hook (e.g., "il2cpp_thread_attach")
 * @param replace_func Pointer to replacement function
 * @param out_orig_func Output: pointer to original function
 * @return Stub handle or NULL on failure
 */
void* memkit_hook_by_symbol(const char* lib_name, const char* symbol_name, void* replace_func, void** out_orig_func);

// ============================================================================
// IL2CPP API (XDL)
// ============================================================================

/**
 * Initialize IL2CPP handle (call once or let resolve auto-initialize)
 * @return true on success, false on failure
 */
bool memkit_il2cpp_init(void);

/**
 * Resolve an IL2CPP export symbol from .dynsym
 * @param symbol_name Symbol name (e.g., "il2cpp_domain_get")
 * @return Function pointer or NULL on failure
 */
void* memkit_il2cpp_resolve(const char* symbol_name);

/**
 * Resolve an IL2CPP symbol from .symtab section only (advanced)
 * Use this for stripped/internal symbols not in .dynsym
 * @param symbol_name Symbol name
 * @return Function pointer or NULL on failure
 */
void* memkit_il2cpp_resolve_symtab(const char* symbol_name);

/**
 * Get cached IL2CPP handle
 * @return Handle pointer or NULL
 */
void* memkit_il2cpp_get_handle(void);

// ============================================================================
// XDL WRAPPER TYPES
// ============================================================================

/**
 * Library information for iteration callbacks
 */
typedef struct {
    const char* name;             // Library basename (e.g., "libil2cpp.so")
    const char* path;             // Full path (may be NULL if not available)
    uintptr_t base;               // Load base address
    size_t size;                  // Library size in bytes
} MemKitLibInfo;

/**
 * Callback type for library iteration
 * @param info Library information
 * @param user_data User context data
 * @return true to continue iteration, false to stop
 */
typedef bool (*memkit_lib_iter_cb_t)(const MemKitLibInfo* info, void* user_data);

/**
 * Symbol information from address resolution
 */
typedef struct {
    const char* lib_name;         // Library pathname
    uintptr_t lib_base;           // Library base address
    const char* sym_name;         // Nearest symbol name (may be NULL)
    uintptr_t sym_offset;         // Offset from symbol start
    size_t sym_size;              // Symbol size in bytes
} MemKitSymInfo;

/**
 * Opaque context for address resolution cache
 * Manages internal xdl_addr() cache to prevent memory leaks
 */
typedef struct memkit_addr_ctx memkit_addr_ctx_t;

// ============================================================================
// XDL FLAG CONSTANTS
// ============================================================================

/* memkit_xdl_open() flags */
#define XDL_DEFAULT           0x00
#define XDL_TRY_FORCE_LOAD    0x01
#define XDL_ALWAYS_FORCE_LOAD 0x02

/* memkit_xdl_addr_to_symbol4() flags */
#define XDL_NON_SYM           0x01

/* memkit_xdl_iterate() flags */
#define XDL_FULL_PATHNAME     0x01

/* xdl_dlinfo() selector */
#define XDL_DI_DLINFO         1

// ============================================================================
// XDL WRAPPER API - PHASE 1: CORE DISCOVERY
// ============================================================================

/**
 * Iterate all loaded shared libraries
 *
 * @param callback Function called for each library
 * @param user_data User context passed to callback
 * @param flags XDL_DEFAULT or XDL_FULL_PATHNAME
 * @return Number of libraries iterated, or -1 on error
 *
 * Example:
 *   typedef struct { const char* target; uintptr_t base; } ctx_t;
 *
 *   bool callback(const MemKitLibInfo* info, void* user_data) {
 *       ctx_t* ctx = (ctx_t*)user_data;
 *       if (strcmp(info->name, ctx->target) == 0) {
 *           ctx->base = info->base;
 *           return false; // Stop iteration
 *       }
 *       return true; // Continue
 *   }
 *
 *   ctx_t ctx = {.target = "libil2cpp.so"};
 *   memkit_xdl_iterate(callback, &ctx, XDL_DEFAULT);
 */
int memkit_xdl_iterate(memkit_lib_iter_cb_t callback, void* user_data, int flags);

/**
 * Open a handle to any loaded library
 *
 * @param name Library name (basename or full path)
 * @param flags XDL_DEFAULT, XDL_TRY_FORCE_LOAD, or XDL_ALWAYS_FORCE_LOAD
 * @return Handle on success, NULL on failure
 *
 * Example:
 *   void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
 *   if (handle) {
 *       void* sym = memkit_xdl_sym(handle, "open", NULL);
 *       memkit_xdl_close(handle);
 *   }
 */
void* memkit_xdl_open(const char* name, int flags);

/**
 * Close a library handle
 *
 * @param handle Handle from memkit_xdl_open()
 * @return true if handle was closed, false if no action needed
 */
bool memkit_xdl_close(void* handle);

/**
 * Resolve a symbol from a library handle (.dynsym section)
 *
 * @param handle Library handle from memkit_xdl_open()
 * @param symbol Symbol name to resolve
 * @param out_size Optional output: symbol size
 * @return Symbol address on success, NULL on failure
 *
 * Example:
 *   void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
 *   size_t size;
 *   void* open_sym = memkit_xdl_sym(handle, "open", &size);
 */
void* memkit_xdl_sym(void* handle, const char* symbol, size_t* out_size);

/**
 * Resolve a debug symbol from a library handle (.symtab section)
 * Use for stripped/internal symbols not in .dynsym
 *
 * @param handle Library handle from memkit_xdl_open()
 * @param symbol Symbol name to resolve
 * @param out_size Optional output: symbol size
 * @return Symbol address on success, NULL on failure
 */
void* memkit_xdl_dsym(void* handle, const char* symbol, size_t* out_size);

/**
 * Get detailed information about a loaded library
 *
 * @param handle Library handle from memkit_xdl_open()
 * @param out Output: library information
 * @return true on success, false on failure
 *
 * Example:
 *   MemKitLibInfo info;
 *   if (memkit_xdl_get_lib_info(handle, &info)) {
 *       // info.base, info.size available
 *   }
 */
bool memkit_xdl_get_lib_info(void* handle, MemKitLibInfo* out);

// ============================================================================
// XDL WRAPPER API - PHASE 2: DEBUG INTROSPECTION
// ============================================================================

/**
 * Create address resolution context
 * Must be destroyed with memkit_xdl_addr_ctx_destroy() to prevent memory leaks
 *
 * @return Context pointer, or NULL on failure
 *
 * Example:
 *   memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
 *   // ... use context for multiple lookups ...
 *   memkit_xdl_addr_ctx_destroy(ctx);
 */
memkit_addr_ctx_t* memkit_xdl_addr_ctx_create(void);

/**
 * Destroy address resolution context and free cache
 *
 * @param ctx Context pointer from memkit_xdl_addr_ctx_create()
 */
void memkit_xdl_addr_ctx_destroy(memkit_addr_ctx_t* ctx);

/**
 * Resolve address to symbol information
 *
 * @param addr Target address to resolve
 * @param out Output: symbol information
 * @param ctx Context from memkit_xdl_addr_ctx_create()
 * @return true on success, false on failure
 *
 * Example:
 *   memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
 *   MemKitSymInfo info;
 *   if (memkit_xdl_addr_to_symbol((void*)0x12345678, &info, ctx)) {
 *       // info.sym_name contains nearest symbol
 *   }
 *   memkit_xdl_addr_ctx_destroy(ctx);
 */
bool memkit_xdl_addr_to_symbol(void* addr, MemKitSymInfo* out, memkit_addr_ctx_t* ctx);

/**
 * Resolve address to symbol with flags (advanced)
 *
 * @param addr Target address
 * @param out Output: symbol information
 * @param ctx Context pointer (may be NULL for single lookup)
 * @param flags XDL_DEFAULT or XDL_NON_SYM (skip symbol lookup)
 * @return true on success, false on failure
 *
 * Use XDL_NON_SYM for fast address-to-library lookup without symbol resolution.
 */
bool memkit_xdl_addr_to_symbol4(void* addr, MemKitSymInfo* out, memkit_addr_ctx_t* ctx, int flags);

// ============================================================================
// XDL WRAPPER API - PHASE 3: ADVANCED FEATURES
// ============================================================================

/**
 * Create a handle from dl_phdr_info (advanced use case)
 * Typically used during library iteration callbacks
 *
 * @param info Pointer to dl_phdr_info structure
 * @return Handle on success, NULL on failure
 *
 * Example:
 *   bool callback(const MemKitLibInfo* info, void* user_data) {
 *       // Create handle without knowing library name
 *       void* handle = memkit_xdl_open_from_phdr(info->phdr_info);
 *   }
 */
void* memkit_xdl_open_from_phdr(struct dl_phdr_info* info);

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

/**
 * IL2CPP_CALL macro - Auto-caches resolved function pointer
 * Uses __builtin_expect for branch prediction optimization
 *
 * Usage: IL2CPP_CALL(return_type, "symbol_name", arg_types...)(arguments...)
 *
 * Example:
 *   void* domain = IL2CPP_CALL(void*, "il2cpp_domain_get")(void);
 *   IL2CPP_CALL(void, "il2cpp_thread_attach", void*)(domain);
 */
#define IL2CPP_CALL(ret_type, func_name, ...) ({ \
    static ret_type (*func_ptr)(__VA_ARGS__) = NULL; \
    if (__builtin_expect(!func_ptr, 0)) { \
        func_ptr = (ret_type (*)(__VA_ARGS__)) memkit_il2cpp_resolve(func_name); \
    } \
    func_ptr; \
})

/**
 * XDL_RESOLVE - Resolve symbol from library (one-shot, auto-closes handle)
 *
 * Usage: void* sym = XDL_RESOLVE("libc.so", "open");
 *
 * Note: This is a convenience macro for quick one-off lookups.
 * For multiple lookups, use memkit_xdl_open() + memkit_xdl_sym() directly.
 */
#define XDL_RESOLVE(lib_name, symbol) \
    memkit_xdl_sym(memkit_xdl_open(lib_name, XDL_DEFAULT), symbol, NULL)

/**
 * XDL_RESOLVE_SIZE - Resolve symbol with size output
 *
 * Usage:
 *   size_t size;
 *   void* sym = XDL_RESOLVE_SIZE("libc.so", "open", &size);
 */
#define XDL_RESOLVE_SIZE(lib_name, symbol, out_size) \
    memkit_xdl_sym(memkit_xdl_open(lib_name, XDL_DEFAULT), symbol, out_size)

// ============================================================================
// HOOKING: ERROR HANDLING
// ============================================================================

/* Error codes (mirrors ShadowHook's 46 error codes) */
#define MK_ERRNO_OK                     0
#define MK_ERRNO_PENDING                1
#define MK_ERRNO_UNINIT                 2
#define MK_ERRNO_INVALID_ARG            3
#define MK_ERRNO_OOM                    4
#define MK_ERRNO_MPROT                  5
#define MK_ERRNO_WRITE_CRASH            6
#define MK_ERRNO_INIT_ERRNO             7
#define MK_ERRNO_INIT_SIGSEGV           8
#define MK_ERRNO_INIT_SIGBUS            9
#define MK_ERRNO_INTERCEPT_DUP          10
#define MK_ERRNO_INIT_SAFE              11
#define MK_ERRNO_INIT_LINKER            12
#define MK_ERRNO_INIT_HUB               13
#define MK_ERRNO_HUB_CREAT              14
#define MK_ERRNO_MONITOR_DLOPEN         15
#define MK_ERRNO_HOOK_UNIQUE_DUP        16
#define MK_ERRNO_HOOK_DLOPEN_CRASH      17
#define MK_ERRNO_HOOK_DLSYM             18
#define MK_ERRNO_HOOK_DLSYM_CRASH       19
#define MK_ERRNO_HOOK_DUP               20
#define MK_ERRNO_HOOK_DLADDR_CRASH      21
#define MK_ERRNO_HOOK_DLINFO            22
#define MK_ERRNO_HOOK_SYMSZ             23
#define MK_ERRNO_HOOK_ENTER             24
#define MK_ERRNO_HOOK_REWRITE_CRASH     25
#define MK_ERRNO_HOOK_REWRITE_FAILED    26
#define MK_ERRNO_UNHOOK_NOTFOUND        27
#define MK_ERRNO_UNHOOK_CMP_CRASH       28
#define MK_ERRNO_UNHOOK_TRAMPO_MISMATCH 29
#define MK_ERRNO_UNHOOK_EXIT_MISMATCH   30
#define MK_ERRNO_UNHOOK_EXIT_CRASH      31
#define MK_ERRNO_UNHOOK_ON_ERROR        32
#define MK_ERRNO_UNHOOK_ON_UNFINISHED   33
#define MK_ERRNO_ELF_ARCH_MISMATCH      34
#define MK_ERRNO_LINKER_ARCH_MISMATCH   35
#define MK_ERRNO_DUP                    36
#define MK_ERRNO_NOT_FOUND              37
#define MK_ERRNO_NOT_SUPPORT            38
#define MK_ERRNO_INIT_TASK              39
#define MK_ERRNO_HOOK_ISLAND_EXIT       40
#define MK_ERRNO_HOOK_ISLAND_ENTER      41
#define MK_ERRNO_HOOK_ISLAND_REWRITE    42
#define MK_ERRNO_MODE_CONFLICT          43
#define MK_ERRNO_HOOK_MULTI_DUP         44
#define MK_ERRNO_DISABLED               45

/* Get last error code from ShadowHook */
int memkit_errno(void);

/* Get human-readable error message */
const char *memkit_strerror(int errno_code);

/* Get ShadowHook version string */
const char *memkit_version(void);

/* Get error code from last shadowhook_init() call */
int memkit_init_errno(void);

// ============================================================================
// HOOKING: MODE CONSTANTS
// ============================================================================

#define MK_MODE_SHARED    0
#define MK_MODE_UNIQUE    1
#define MK_MODE_MULTI     2

// ============================================================================
// HOOKING: CPU CONTEXT & INTERCEPT TYPES
// ============================================================================

/* CPU context passed to interceptor — direct passthrough to ShadowHook */
typedef shadowhook_cpu_context_t MemKitCpuContext;

/* NEON/VFP vector register — direct passthrough to ShadowHook */
typedef shadowhook_vreg_t MemKitVReg;

/* Interceptor function type — receives CPU context on each call to target */
typedef void (*MemKitInterceptor)(
    MemKitCpuContext *cpu_context,
    void *data
);

// ============================================================================
// HOOKING: CALLBACK TYPES
// ============================================================================

/* Callback invoked when a hook operation completes (success or failure) */
typedef void (*MemKitHooked)(
    int error_number,
    const char *lib_name,
    const char *sym_name,
    void *sym_addr,
    void *new_addr,
    void *orig_addr,
    void *arg
);

/* Callback invoked when an intercept operation completes */
typedef void (*MemKitIntercepted)(
    int error_number,
    const char *lib_name,
    const char *sym_name,
    void *sym_addr,
    void *pre,
    void *data,
    void *arg
);

// ============================================================================
// HOOKING: PROXY & STACK MANAGEMENT (Macros)
// ============================================================================

/* Call the previous function in the proxy chain (MULTI mode only).
 * @param func       Your proxy function pointer
 * @param func_sig   Function signature type, e.g., int(*)(int, const char*)
 * @param ...        Arguments to forward
 * @note Only works in MULTI mode. In SHARED mode, use SHADOWHOOK_CALL_PREV directly. */
#define MEMKIT_CALL_PREV(func, func_sig, ...) \
    ((func_sig)memkit_get_prev_func((void *)(func)))(__VA_ARGS__)

/* Pop the current stack frame after a proxy call returns.
 * Must be called at the end of every proxy function. */
#define MEMKIT_POP_STACK() \
    memkit_pop_stack(__builtin_return_address(0))

/* Allow reentrant calls to this proxy from the same thread. */
#define MEMKIT_ALLOW_REENTRANT() \
    memkit_allow_reentrant(__builtin_return_address(0))

/* Disallow reentrant calls to this proxy from the same thread. */
#define MEMKIT_DISALLOW_REENTRANT() \
    memkit_disallow_reentrant(__builtin_return_address(0))

/* Get the return address of the current proxy caller. */
#define MEMKIT_RETURN_ADDRESS() \
    memkit_get_return_address()

// ============================================================================
// HOOKING: PROXY & STACK MANAGEMENT (Functions)
// ============================================================================

void *memkit_get_prev_func(void *func);
void memkit_pop_stack(const void *return_address);
void memkit_allow_reentrant(const void *return_address);
void memkit_disallow_reentrant(const void *return_address);
void *memkit_get_return_address(void);

// ============================================================================
// HOOKING: FLAGS (V2 API)
// ============================================================================

#define MK_HOOK_DEFAULT                 0
#define MK_HOOK_WITH_SHARED_MODE        1
#define MK_HOOK_WITH_UNIQUE_MODE        2
#define MK_HOOK_WITH_MULTI_MODE         4
#define MK_HOOK_RECORD                  8

void *memkit_hook_v2(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, uint32_t flags);
void *memkit_hook_by_symbol_v2(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, uint32_t flags);

// ============================================================================
// INTERCEPT API
// ============================================================================

/* Intercept flags */
#define MK_INTERCEPT_DEFAULT                0
#define MK_INTERCEPT_WITH_FPSIMD_READ_ONLY  1
#define MK_INTERCEPT_WITH_FPSIMD_WRITE_ONLY 2
#define MK_INTERCEPT_WITH_FPSIMD_READ_WRITE 3
#define MK_INTERCEPT_RECORD                 4

/* Intercept by function address */
void *memkit_intercept(void *func_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...);

/* Intercept by symbol address */
void *memkit_intercept_by_sym_addr(void *sym_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...);

/* Intercept by library name and symbol name */
void *memkit_intercept_by_symbol(const char *lib_name, const char *sym_name, MemKitInterceptor pre, void *data, uint32_t flags);

/* Remove an interceptor */
int memkit_unintercept(void *stub);

/* Intercept at a specific instruction address */
void *memkit_intercept_at_instr(void *instr_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...);

/* Intercept with completion callback */
void *memkit_intercept_with_callback(const char *lib_name, const char *sym_name, MemKitInterceptor pre, void *data, uint32_t flags, MemKitIntercepted intercepted, void *arg);

// ============================================================================
// RECORDS API
// ============================================================================

#define MK_RECORD_ITEM_TIMESTAMP        (1 << 0)
#define MK_RECORD_ITEM_CALLER_LIB_NAME  (1 << 1)
#define MK_RECORD_ITEM_OP               (1 << 2)
#define MK_RECORD_ITEM_LIB_NAME         (1 << 3)
#define MK_RECORD_ITEM_SYM_NAME         (1 << 4)
#define MK_RECORD_ITEM_SYM_ADDR         (1 << 5)
#define MK_RECORD_ITEM_NEW_ADDR         (1 << 6)
#define MK_RECORD_ITEM_BACKUP_LEN       (1 << 7)
#define MK_RECORD_ITEM_ERRNO            (1 << 8)
#define MK_RECORD_ITEM_STUB             (1 << 9)
#define MK_RECORD_ITEM_FLAGS            (1 << 10)
#define MK_RECORD_ITEM_ALL              0x7FF

/* Get operation records as CSV string (caller must free) */
/* NOTE: The returned string is heap-allocated; caller is responsible for calling free() */
char *memkit_get_records(uint32_t item_flags);

/* Dump operation records to a file descriptor */
void memkit_dump_records_fd(int fd, uint32_t item_flags);

// ============================================================================
// RUNTIME CONFIGURATION
// ============================================================================

#define MK_IS_SHARED_MODE (MK_MODE_SHARED == memkit_get_mode())
#define MK_IS_UNIQUE_MODE (MK_MODE_UNIQUE == memkit_get_mode())
#define MK_IS_MULTI_MODE  (MK_MODE_MULTI  == memkit_get_mode())

int memkit_get_mode(void);
void memkit_set_debuggable(bool debuggable);
bool memkit_get_debuggable(void);
void memkit_set_recordable(bool recordable);
bool memkit_get_recordable(void);
void memkit_set_disable(bool disable);
bool memkit_get_disable(void);

// ============================================================================
// DL INIT/FINI CALLBACKS
// ============================================================================

typedef shadowhook_dl_info_t MemKitDlInfo;
typedef void (*MemKitDlInitCallback)(struct dl_phdr_info *info, size_t size, void *data);
typedef void (*MemKitDlFiniCallback)(struct dl_phdr_info *info, size_t size, void *data);

int memkit_register_dl_init_callback(MemKitDlInitCallback pre, MemKitDlInitCallback post, void *data);
int memkit_unregister_dl_init_callback(MemKitDlInitCallback pre, MemKitDlInitCallback post, void *data);
int memkit_register_dl_fini_callback(MemKitDlFiniCallback pre, MemKitDlFiniCallback post, void *data);
int memkit_unregister_dl_fini_callback(MemKitDlFiniCallback pre, MemKitDlFiniCallback post, void *data);

// ============================================================================
// HOOKING: CALLBACK VARIANTS
// ============================================================================

/* Hook with completion callback */
void *memkit_hook_with_callback(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, MemKitHooked hooked, void *hooked_arg);

/* Hook by symbol name with completion callback (alias) */
void *memkit_hook_by_symbol_callback(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr, MemKitHooked hooked, void *hooked_arg);

#ifdef __cplusplus
}
#endif

#endif // MEMKIT_H
