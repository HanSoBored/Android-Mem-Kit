# Android-Mem-Kit Usage Guide

## Table of Contents

1. [Getting Started](#getting-started)
2. [API Reference](#api-reference)
3. [Memory Patching](#memory-patching)
4. [Function Hooking](#function-hooking)
5. [IL2CPP Instrumentation](#il2cpp-instrumentation)
6. [Error Handling](#error-handling)
7. [Best Practices](#best-practices)

---

## Getting Started

### Quick Setup

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
```

### Initialization Sequence

```c
__attribute__((constructor))
void init() {
    // Step 1: Initialize ShadowHook (required before any hooking)
    int ret = memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);
    if (ret != 0) {
        LOGE("ShadowHook init failed: %d", ret);
        return;
    }

    // Step 2: Wait for target library to load
    uintptr_t base = 0;
    for (int i = 0; i < 30 && base == 0; i++) {
        base = memkit_get_lib_base("libtarget.so");
        if (base == 0) sleep(1);
    }

    if (base == 0) {
        LOGE("Target library not found");
        return;
    }

    LOGI("Library base: 0x%lx", base);

    // Step 3: Start your instrumentation
    start_instrumentation(base);
}
```

---

## API Reference

### Memory Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_get_lib_base(const char* lib_name)` | Get lowest base address of loaded library | `uintptr_t` or 0 |
| `memkit_patch_create(addr, hex_string)` | Create memory patch from hex string | `MemPatch*` or NULL |
| `memkit_patch_apply(patch)` | Apply memory patch | `bool` |
| `memkit_patch_restore(patch)` | Restore original bytes | `bool` |
| `memkit_patch_free(patch)` | Free patch resources | `void` |

### Hooking Functions

#### Basic Hook API

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_hook_init(mode, debuggable)` | Initialize ShadowHook | `int` (0 = success) |
| `memkit_hook(addr, replace, &orig)` | Hook function by address | `stub` or NULL |
| `memkit_hook_by_symbol(lib, sym, func, &orig)` | Hook by symbol name | `stub` or NULL |
| `memkit_unhook(stub)` | Unhook function | `void` |

#### V2 Hook API

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_hook_v2(lib, sym, new, &orig, flags)` | Hook with mode flags | `stub` or NULL |
| `memkit_hook_by_symbol_v2(lib, sym, new, &orig, flags)` | Hook by symbol with flags | `stub` or NULL |

**V2 Hook Flags:**

| Flag | Description |
|------|-------------|
| `MK_HOOK_DEFAULT` | Default behavior (respects init mode) |
| `MK_HOOK_WITH_SHARED_MODE` | Force SHARED mode for this hook |
| `MK_HOOK_WITH_UNIQUE_MODE` | Force UNIQUE mode for this hook |
| `MK_HOOK_WITH_MULTI_MODE` | Force MULTI mode for this hook |
| `MK_HOOK_RECORD` | Enable recording for this hook operation |

#### Hook with Callback

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_hook_with_callback(lib, sym, new, &orig, cb, arg)` | Hook with completion callback | `stub` or NULL |
| `memkit_hook_by_symbol_callback(lib, sym, new, &orig, cb, arg)` | Alias for above | `stub` or NULL |

**Callback Types:**

| Type | Description |
|------|-------------|
| `MemKitHooked` | Called when hook operation completes (success or failure) |
| `MemKitIntercepted` | Called when intercept operation completes |
| `MemKitInterceptor` | Receives CPU context on each call to target |

#### Intercept API

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_intercept(addr, pre, data, flags, ...)` | Intercept by address | `stub` or NULL |
| `memkit_intercept_by_symbol(lib, sym, pre, data, flags)` | Intercept by symbol | `stub` or NULL |
| `memkit_intercept_by_sym_addr(addr, pre, data, flags, ...)` | Intercept by symbol address | `stub` or NULL |
| `memkit_intercept_at_instr(addr, pre, data, flags, ...)` | Intercept at specific instruction | `stub` or NULL |
| `memkit_intercept_with_callback(lib, sym, pre, data, flags, cb, arg)` | Intercept with callback | `stub` or NULL |
| `memkit_unintercept(stub)` | Remove interceptor | `int` |

**Intercept Flags:**

| Flag | Description |
|------|-------------|
| `MK_INTERCEPT_DEFAULT` | Standard intercept (no FP/SIMD context) |
| `MK_INTERCEPT_WITH_FPSIMD_READ_ONLY` | Include FP/SIMD registers (read-only) |
| `MK_INTERCEPT_WITH_FPSIMD_WRITE_ONLY` | Include FP/SIMD registers (write-only) |
| `MK_INTERCEPT_WITH_FPSIMD_READ_WRITE` | Include FP/SIMD registers (read-write) |
| `MK_INTERCEPT_RECORD` | Enable recording for this intercept |

**Context Types:**

| Type | Description |
|------|-------------|
| `MemKitCpuContext` | CPU context passed to interceptor |
| `MemKitVReg` | NEON/VFP vector register |

#### Records API

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_get_records(item_flags)` | Get records as CSV string | `char*` (caller frees) |
| `memkit_dump_records_fd(fd, item_flags)` | Dump records to file descriptor | `void` |

**Record Item Flags:**

| Flag | Description |
|------|-------------|
| `MK_RECORD_ITEM_TIMESTAMP` | Include timestamp |
| `MK_RECORD_ITEM_CALLER_LIB_NAME` | Include caller library name |
| `MK_RECORD_ITEM_OP` | Include operation type |
| `MK_RECORD_ITEM_LIB_NAME` | Include target library name |
| `MK_RECORD_ITEM_SYM_NAME` | Include symbol name |
| `MK_RECORD_ITEM_SYM_ADDR` | Include symbol address |
| `MK_RECORD_ITEM_NEW_ADDR` | Include new function address |
| `MK_RECORD_ITEM_BACKUP_LEN` | Include backup length |
| `MK_RECORD_ITEM_ERRNO` | Include error code |
| `MK_RECORD_ITEM_STUB` | Include stub pointer |
| `MK_RECORD_ITEM_FLAGS` | Include flags |
| `MK_RECORD_ITEM_ALL` | Include all fields (0x7FF) |

#### Runtime Configuration

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_get_mode()` | Get current hooking mode | `int` |
| `memkit_set_debuggable(val)` | Enable/disable debug logging | `void` |
| `memkit_get_debuggable()` | Check debug mode status | `bool` |
| `memkit_set_recordable(val)` | Enable/disable recording | `void` |
| `memkit_get_recordable()` | Check recording status | `bool` |
| `memkit_set_disable(val)` | Global enable/disable switch | `void` |
| `memkit_get_disable()` | Check disabled status | `bool` |

**Mode Check Macros:**

| Macro | Description |
|-------|-------------|
| `MK_IS_SHARED_MODE` | Evaluates to true if in SHARED mode |
| `MK_IS_UNIQUE_MODE` | Evaluates to true if in UNIQUE mode |
| `MK_IS_MULTI_MODE` | Evaluates to true if in MULTI mode |

#### DL Callbacks

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_register_dl_init_callback(pre, post, data)` | Register dlopen callback | `int` |
| `memkit_unregister_dl_init_callback(pre, post, data)` | Unregister dlopen callback | `int` |
| `memkit_register_dl_fini_callback(pre, post, data)` | Register dlclose callback | `int` |
| `memkit_unregister_dl_fini_callback(pre, post, data)` | Unregister dlclose callback | `int` |

**DL Callback Types:**

| Type | Description |
|------|-------------|
| `MemKitDlInfo` | Library info for DL callbacks |
| `MemKitDlInitCallback` | Called when library is loaded (dlopen) |
| `MemKitDlFiniCallback` | Called when library is unloaded (dlclose) |

#### Proxy/Stack Macros

| Macro | Description |
|-------|-------------|
| `MEMKIT_CALL_PREV(func, func_sig, ...)` | Call previous function in proxy chain (MULTI mode) |
| `MEMKIT_POP_STACK()` | Pop current stack frame after proxy call |
| `MEMKIT_ALLOW_REENTRANT()` | Allow reentrant calls from same thread |
| `MEMKIT_DISALLOW_REENTRANT()` | Disallow reentrant calls from same thread |
| `MEMKIT_RETURN_ADDRESS()` | Get return address of current proxy caller |

#### Proxy/Stack Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_get_prev_func(func)` | Get previous function pointer | `void*` |
| `memkit_pop_stack(return_addr)` | Pop stack frame | `void` |
| `memkit_allow_reentrant(return_addr)` | Allow reentrancy | `void` |
| `memkit_disallow_reentrant(return_addr)` | Disallow reentrancy | `void` |
| `memkit_get_return_address()` | Get caller return address | `void*` |

### IL2CPP Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_il2cpp_init()` | Initialize IL2CPP handle | `bool` |
| `memkit_il2cpp_resolve(symbol)` | Resolve from .dynsym | `void*` or NULL |
| `memkit_il2cpp_resolve_symtab(symbol)` | Resolve from .symtab | `void*` or NULL |
| `memkit_il2cpp_get_handle()` | Get cached handle | `void*` or NULL |
| `IL2CPP_CALL(ret, name, ...)` | Macro for auto-cached calls | Function pointer |

### XDL Wrapper Functions

#### Library Discovery

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_iterate(cb, data, flags)` | Iterate all loaded libraries | `int` (count or -1) |
| `memkit_xdl_open(name, flags)` | Open library handle | `void*` or NULL |
| `memkit_xdl_close(handle)` | Close library handle | `bool` |
| `memkit_xdl_get_lib_info(handle, &info)` | Get library details | `bool` |

#### Symbol Resolution

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_sym(handle, symbol, &size)` | Resolve from .dynsym | `void*` or NULL |
| `memkit_xdl_dsym(handle, symbol, &size)` | Resolve from .symtab (debug) | `void*` or NULL |

#### Address-to-Symbol (Debug Introspection)

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_addr_ctx_create()` | Create resolution context | `ctx*` or NULL |
| `memkit_xdl_addr_ctx_destroy(ctx)` | Destroy context | `void` |
| `memkit_xdl_addr_to_symbol(addr, &info, ctx)` | Resolve address to symbol | `bool` |
| `memkit_xdl_addr_to_symbol4(addr, &info, ctx, flags)` | With flags (e.g., `XDL_NON_SYM`) | `bool` |

#### Advanced

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_open_from_phdr(info)` | Create handle from `dl_phdr_info` | `void*` or NULL |

#### Convenience Macros

| Macro | Description |
|-------|-------------|
| `XDL_RESOLVE(lib, sym)` | One-shot symbol resolve |
| `XDL_RESOLVE_SIZE(lib, sym, &size)` | Resolve with size output |

---

## Memory Patching

### Basic Patch

```c
// ARM64: MOV X0, #0 (returns 0)
uintptr_t base = memkit_get_lib_base("libtarget.so");
MemPatch* patch = memkit_patch_create(base + 0x1234, "00 00 80 D2");

if (patch && memkit_patch_apply(patch)) {
    LOGI("Patch applied successfully");
} else {
    LOGE("Patch failed: %d", errno);
}

// Restore later if needed
// memkit_patch_restore(patch);

// Free when done
// memkit_patch_free(patch);
```

### Multi-Byte Patch

```c
// ARM64: Multiple instructions
MemPatch* multi_patch = memkit_patch_create(
    base + 0x5678,
    "00 00 80 D2 20 00 80 D2 1F 20 03 D5"  // MOV X0,#0; MOV X0,#1; NOP
);

if (multi_patch && memkit_patch_apply(multi_patch)) {
    LOGI("Multi-byte patch applied");
}
```

### Cross-Page Boundary (Safe)

The library automatically handles patches that span memory pages:

```c
// This 20-byte patch might span two pages
// memkit handles this automatically
MemPatch* cross_page = memkit_patch_create(
    base + 0xFFF0,  // Near page boundary
    "00 00 80 D2 20 00 80 D2 40 00 80 D2 60 00 80 D2 80 00 80 D2"
);
```

---

## Function Hooking

### Hook by Symbol Name (Recommended)

```c
// Original function pointer
static int (*orig_target_function)(int param) = NULL;
static void* hook_stub = NULL;

// Replacement function
static int my_target_function(int param) {
    LOGI("target_function called with: %d", param);

    // Call original if needed
    return orig_target_function(param);
}

// Hook it
hook_stub = memkit_hook_by_symbol(
    "libtarget.so",
    "target_function",
    (void*)my_target_function,
    (void**)&orig_target_function
);

if (hook_stub) {
    LOGI("Hook successful");
} else {
    LOGE("Hook failed: %d", errno);
}
```

### Hook by Address

```c
uintptr_t func_addr = base + 0xABCD;

hook_stub = memkit_hook(
    func_addr,
    (void*)my_function,
    (void**)&orig_function
);
```

### Unhook

```c
// When done, unhook to restore original behavior
memkit_unhook(hook_stub);
```

### ShadowHook Macros

MemKit provides convenient macros for proxy/stack management:

```c
// In MULTI mode: call the previous function in the proxy chain
int my_proxy(int a, const char* b) {
    int ret = MEMKIT_CALL_PREV(my_proxy, int(*)(int, const char*), a, b);
    MEMKIT_POP_STACK();  // Must call at end of every proxy
    return ret;
}

// Control reentrancy within a proxy
MEMKIT_ALLOW_REENTRANT();    // Allow recursive calls from same thread
MEMKIT_DISALLOW_REENTRANT(); // Block recursive calls

// Get the return address of the current proxy caller
void* ret_addr = MEMKIT_RETURN_ADDRESS();
```

### V2 Hook API

The V2 API allows per-hook mode control with flags:

```c
// Hook with specific mode (overrides global init mode)
void* stub = memkit_hook_v2(
    "libtarget.so",
    "target_function",
    (void*)my_function,
    (void**)&orig_function,
    MK_HOOK_WITH_UNIQUE_MODE  // Force UNIQUE for this hook only
);

// Hook with recording enabled
void* recorded_stub = memkit_hook_by_symbol_v2(
    "libssl.so",
    "SSL_read",
    (void*)my_SSL_read,
    (void**)&orig_SSL_read,
    MK_HOOK_RECORD  // Log this hook operation
);

// Combine flags
void* stub = memkit_hook_v2(
    "libtarget.so",
    "check_integrity",
    (void*)my_check,
    (void**)&orig_check,
    MK_HOOK_WITH_SHARED_MODE | MK_HOOK_RECORD
);
```

### Hook with Callback

Get notified when a hook operation completes:

```c
// Completion callback — called after hook is installed (or fails)
void on_hook_complete(int error_number, const char* lib_name,
                      const char* sym_name, void* sym_addr,
                      void* new_addr, void* orig_addr, void* arg) {
    if (error_number == 0) {
        LOGI("Hook installed: %s!%s at %p", lib_name, sym_name, sym_addr);
    } else {
        LOGE("Hook failed: %d — %s", error_number, memkit_strerror(error_number));
    }
}

// Hook with callback
void* stub = memkit_hook_with_callback(
    "libtarget.so",
    "target_function",
    (void*)my_function,
    (void**)&orig_function,
    on_hook_complete,
    NULL  // user arg passed to callback
);

// Same as above (alias)
void* stub2 = memkit_hook_by_symbol_callback(
    "libssl.so", "SSL_read",
    (void*)my_SSL_read, (void**)&orig_SSL_read,
    on_hook_complete, NULL
);
```

### Intercept API

The Intercept API allows you to inspect and modify CPU registers **before** the target function executes. Unlike hooks, interceptors receive the full CPU context and can modify arguments in-place.

```c
// Interceptor function — receives CPU context on each call
static void my_interceptor(MemKitCpuContext* cpu_context, void* data) {
    // Read arguments from registers (ARM64: x0-x7 hold first 8 args)
    uint64_t arg0 = cpu_context->regs[0];
    uint64_t arg1 = cpu_context->regs[1];

    LOGI("Intercepted! arg0=0x%lx, arg1=0x%lx", arg0, arg1);

    // Modify arguments before the target function sees them
    cpu_context->regs[0] = 0;  // Zero out first argument

    // Optionally skip calling the original function entirely
    // by setting the PC to the return address
}

// Basic intercept by symbol
void* stub = memkit_intercept_by_symbol(
    "libtarget.so",
    "target_function",
    my_interceptor,
    NULL,  // user data
    MK_INTERCEPT_DEFAULT
);

// Intercept with FP/SIMD context (for functions using NEON)
void* stub = memkit_intercept_by_symbol(
    "libtarget.so",
    "simd_function",
    my_interceptor,
    NULL,
    MK_INTERCEPT_WITH_FPSIMD_READ_WRITE  // Include vfp/regs
);

// Remove interceptor
memkit_unintercept(stub);
```

#### Intercept with Completion Callback

```c
void on_intercept_complete(int error_number, const char* lib_name,
                           const char* sym_name, void* sym_addr,
                           void* pre, void* data, void* arg) {
    if (error_number == 0) {
        LOGI("Intercept installed: %s!%s", lib_name, sym_name);
    }
}

void* stub = memkit_intercept_with_callback(
    "libtarget.so",
    "check_signature",
    my_interceptor,
    NULL,
    MK_INTERCEPT_DEFAULT,
    on_intercept_complete,
    NULL
);
```

#### Intercept at Specific Instruction

For advanced use cases where you need to intercept at a specific instruction offset:

```c
uintptr_t base = memkit_get_lib_base("libtarget.so");
void* instr_addr = (void*)(base + 0x1234);  // Specific instruction

void* stub = memkit_intercept_at_instr(
    instr_addr,
    my_interceptor,
    NULL,
    MK_INTERCEPT_DEFAULT
);
```

#### Real-World Example: SSL Pinning Bypass via Intercept

```c
// Intercept SSL_CTX_set_verify to force VERIFY_NONE
static void intercept_SSL_CTX_set_verify(MemKitCpuContext* ctx, void* data) {
    // ARM64 calling convention: x0=ctx, x1=mode, x2=callback
    uint64_t mode = ctx->regs[1];
    LOGI("SSL_CTX_set_verify called with mode=0x%lx", mode);

    // Force mode to VERIFY_NONE (0)
    ctx->regs[1] = 0;
}

void* stub = memkit_intercept_by_symbol(
    "libssl.so",
    "SSL_CTX_set_verify",
    intercept_SSL_CTX_set_verify,
    NULL,
    MK_INTERCEPT_DEFAULT
);
```

#### Real-World Example: Integrity Check Bypass via Intercept

```c
// Intercept a signature verification function
static void intercept_verify_signature(MemKitCpuContext* ctx, void* data) {
    // Force return value to 1 (valid) by modifying x0 before return
    // We can't modify return value directly in interceptor,
    // but we can use a hook instead for post-call modification.
    // Interceptors are best for argument inspection/modification.
    LOGI("verify_signature called — arguments logged");
}
```

### Records API

MemKit can log all hook/intercept operations to a CSV format for analysis:

```c
// Enable recording globally
memkit_set_recordable(true);

// Get records as CSV string (caller must free)
char* csv = memkit_get_records(MK_RECORD_ITEM_ALL);
if (csv) {
    LOGI("Operation records:\n%s", csv);
    free(csv);
}

// Dump records directly to a file descriptor
int fd = open("/data/local/tmp/memkit_records.csv", O_WRONLY | O_CREAT, 0644);
if (fd >= 0) {
    memkit_dump_records_fd(fd, MK_RECORD_ITEM_ALL);
    close(fd);
}

// Selective recording — only include specific fields
uint32_t flags = MK_RECORD_ITEM_TIMESTAMP
               | MK_RECORD_ITEM_LIB_NAME
               | MK_RECORD_ITEM_SYM_NAME
               | MK_RECORD_ITEM_ERRNO;
char* csv = memkit_get_records(flags);
```

#### Record Item Flags

| Flag | Field |
|------|-------|
| `MK_RECORD_ITEM_TIMESTAMP` | Timestamp of operation |
| `MK_RECORD_ITEM_CALLER_LIB_NAME` | Library that initiated the operation |
| `MK_RECORD_ITEM_OP` | Operation type (hook, intercept, unhook) |
| `MK_RECORD_ITEM_LIB_NAME` | Target library name |
| `MK_RECORD_ITEM_SYM_NAME` | Target symbol name |
| `MK_RECORD_ITEM_SYM_ADDR` | Symbol address |
| `MK_RECORD_ITEM_NEW_ADDR` | Replacement function address |
| `MK_RECORD_ITEM_BACKUP_LEN` | Backup length of trampoline |
| `MK_RECORD_ITEM_ERRNO` | Error code (0 = success) |
| `MK_RECORD_ITEM_STUB` | Stub pointer |
| `MK_RECORD_ITEM_FLAGS` | Flags used |
| `MK_RECORD_ITEM_ALL` | All fields (0x7FF) |

### Runtime Configuration

Control MemKit behavior at runtime:

```c
// Check current mode
int mode = memkit_get_mode();
if (MK_IS_UNIQUE_MODE) {
    LOGI("Running in UNIQUE mode");
} else if (MK_IS_SHARED_MODE) {
    LOGI("Running in SHARED mode");
}

// Toggle debug logging
memkit_set_debuggable(true);
bool is_debug = memkit_get_debuggable();

// Toggle recording at runtime
memkit_set_recordable(true);
bool is_recordable = memkit_get_recordable();

// Global disable — suspends all hooking/intercepting
memkit_set_disable(true);   // Disable all operations
bool is_disabled = memkit_get_disable();
memkit_set_disable(false);  // Re-enable
```

### DL Callbacks

Register callbacks to be notified when libraries are loaded or unloaded:

```c
// Called before and after a library is loaded (dlopen)
void dl_init_pre(struct dl_phdr_info* info, size_t size, void* data) {
    LOGI("Library loading: %s", info->dlpi_name);
}

void dl_init_post(struct dl_phdr_info* info, size_t size, void* data) {
    LOGI("Library loaded: %s at base 0x%lx", info->dlpi_name, info->dlpi_addr);
}

// Called before and after a library is unloaded (dlclose)
void dl_fini_pre(struct dl_phdr_info* info, size_t size, void* data) {
    LOGI("Library unloading: %s", info->dlpi_name);
}

void dl_fini_post(struct dl_phdr_info* info, size_t size, void* data) {
    LOGI("Library unloaded: %s", info->dlpi_name);
}

// Register callbacks
memkit_register_dl_init_callback(dl_init_pre, dl_init_post, NULL);
memkit_register_dl_fini_callback(dl_fini_pre, dl_fini_post, NULL);

// Unregister when done
memkit_unregister_dl_init_callback(dl_init_pre, dl_init_post, NULL);
memkit_unregister_dl_fini_callback(dl_fini_pre, dl_fini_post, NULL);
```

#### Real-World Example: Auto-Hook When Target Library Loads

```c
// Auto-hook SSL functions when libssl.so is loaded
static void on_ssl_loaded(struct dl_phdr_info* info, size_t size, void* data) {
    if (strstr(info->dlpi_name, "libssl.so")) {
        LOGI("libssl.so detected — installing hooks...");
        // Install hooks here
    }
}

memkit_register_dl_init_callback(NULL, on_ssl_loaded, NULL);
```

---

## IL2CPP Instrumentation

### Basic IL2CPP Call

```c
// Auto-cached function call
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");

if (il2cpp_domain_get) {
    void* domain = il2cpp_domain_get();
    LOGI("IL2CPP Domain: %p", domain);
}
```

### Multiple Calls

```c
// Get domain
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");
void* domain = il2cpp_domain_get();

// Attach thread
void* (*il2cpp_thread_attach)(void*) = IL2CPP_CALL(void*, "il2cpp_thread_attach", void*);
il2cpp_thread_attach(domain);

// Get root namespace
void* (*il2cpp_get_root_namespace)(void) = IL2CPP_CALL(void*, "il2cpp_get_root_namespace");
void* root_ns = il2cpp_get_root_namespace();
```

### Resolve Internal Symbols

```c
// Some symbols are only in .symtab
void* internal_func = memkit_il2cpp_resolve_symtab("_ZN6Player13InternalInitEv");

if (internal_func) {
    LOGI("Found internal function: %p", internal_func);
}
```

### Hook IL2CPP Functions

```c
static void* (*orig_il2cpp_thread_attach)(void*) = NULL;
static void* thread_hook_stub = NULL;

static void* my_il2cpp_thread_attach(void* domain) {
    LOGI("il2cpp_thread_attach called");
    return orig_il2cpp_thread_attach(domain);
}

// Hook it
thread_hook_stub = memkit_hook_by_symbol(
    "libil2cpp.so",
    "il2cpp_thread_attach",
    (void*)my_il2cpp_thread_attach,
    (void**)&orig_il2cpp_thread_attach
);
```

---

## Error Handling

### MemKit Error Functions

MemKit provides its own error handling layer wrapping ShadowHook:

```c
#include <errno.h>
#include <string.h>

// Get last error code from ShadowHook
int err = memkit_errno();

// Get human-readable error message
const char* msg = memkit_strerror(err);
LOGE("Operation failed: %d - %s", err, msg);

// Get version string
const char* version = memkit_version();
LOGI("ShadowHook version: %s", version);

// Get error from last shadowhook_init() call
int init_err = memkit_init_errno();
```

### MK_ERRNO_* Constants

MemKit exposes all 46 ShadowHook error codes:

```c
// Common error codes
MK_ERRNO_OK                     // 0: Success
MK_ERRNO_UNINIT                 // 2: Not initialized
MK_ERRNO_INVALID_ARG            // 3: Invalid argument
MK_ERRNO_OOM                    // 4: Out of memory
MK_ERRNO_MPROT                  // 5: mprotect failed
MK_ERRNO_HOOK_DLSYM             // 18: Symbol not found
MK_ERRNO_HOOK_ENTER             // 24: Failed to enter hook
MK_ERRNO_HOOK_DUP               // 20: Duplicate hook
MK_ERRNO_UNHOOK_NOTFOUND        // 27: Unhook target not found
MK_ERRNO_DISABLED               // 45: Operations disabled

// Full list available in memkit.h:
// MK_ERRNO_PENDING, MK_ERRNO_WRITE_CRASH, MK_ERRNO_INIT_ERRNO,
// MK_ERRNO_INIT_SIGSEGV, MK_ERRNO_INIT_SIGBUS, MK_ERRNO_INTERCEPT_DUP,
// MK_ERRNO_INIT_SAFE, MK_ERRNO_INIT_LINKER, MK_ERRNO_INIT_HUB,
// MK_ERRNO_HUB_CREAT, MK_ERRNO_MONITOR_DLOPEN, MK_ERRNO_HOOK_UNIQUE_DUP,
// MK_ERRNO_HOOK_DLOPEN_CRASH, MK_ERRNO_HOOK_DLSYM_CRASH,
// MK_ERRNO_HOOK_DLADDR_CRASH, MK_ERRNO_HOOK_DLINFO, MK_ERRNO_HOOK_SYMSZ,
// MK_ERRNO_HOOK_REWRITE_CRASH, MK_ERRNO_HOOK_REWRITE_FAILED,
// MK_ERRNO_UNHOOK_CMP_CRASH, MK_ERRNO_UNHOOK_TRAMPO_MISMATCH,
// MK_ERRNO_UNHOOK_EXIT_MISMATCH, MK_ERRNO_UNHOOK_EXIT_CRASH,
// MK_ERRNO_UNHOOK_ON_ERROR, MK_ERRNO_UNHOOK_ON_UNFINISHED,
// MK_ERRNO_ELF_ARCH_MISMATCH, MK_ERRNO_LINKER_ARCH_MISMATCH,
// MK_ERRNO_DUP, MK_ERRNO_NOT_FOUND, MK_ERRNO_NOT_SUPPORT,
// MK_ERRNO_INIT_TASK, MK_ERRNO_HOOK_ISLAND_EXIT,
// MK_ERRNO_HOOK_ISLAND_ENTER, MK_ERRNO_HOOK_ISLAND_REWRITE,
// MK_ERRNO_MODE_CONFLICT, MK_ERRNO_HOOK_MULTI_DUP
```

### Using memkit_strerror()

```c
void* stub = memkit_hook_by_symbol("lib.so", "func", my_func, (void**)&orig);
if (stub == NULL) {
    int err = memkit_errno();
    const char* msg = memkit_strerror(err);
    LOGE("Hook failed: %d - %s", err, msg);

    // Common troubleshooting:
    if (err == MK_ERRNO_HOOK_DLSYM) {
        LOGE("Symbol not found — check library name and symbol spelling");
    } else if (err == MK_ERRNO_UNINIT) {
        LOGE("ShadowHook not initialized — call memkit_hook_init() first");
    } else if (err == MK_ERRNO_INVALID_ARG) {
        LOGE("Invalid argument — check function pointers and addresses");
    }
}
```

### ShadowHook Error Codes (Legacy)

For compatibility, you can still use ShadowHook's native error functions:

```c
if (stub == NULL) {
    int err = shadowhook_get_errno();
    const char* msg = shadowhook_to_errmsg(err);
    LOGE("ShadowHook error: %d - %s", err, msg);
}
```

---

## Best Practices

### 1. Thread Safety

The library is thread-safe. You can call APIs from multiple threads:

```c
// Safe to call from any thread
void* thread_func(void* arg) {
    void* func = IL2CPP_CALL(void*, "some_function");
    if (func) func();
    return NULL;
}
```

### 2. Memory Management

Always free patches when done:

```c
MemPatch* patch = memkit_patch_create(...);
memkit_patch_apply(patch);

// ... later ...
memkit_patch_restore(patch);
memkit_patch_free(patch);
```

### 3. Wait for Library Load

```c
uintptr_t wait_for_lib(const char* name, int timeout_sec) {
    uintptr_t base = 0;
    for (int i = 0; i < timeout_sec && base == 0; i++) {
        base = memkit_get_lib_base(name);
        if (base == 0) sleep(1);
    }
    return base;
}
```

### 4. Validate Pointers

```c
// Always check for NULL before using
if (orig_function != NULL) {
    orig_function(param);
}

// Check patch before applying
if (patch && memkit_patch_apply(patch)) {
    // Success
}
```

### 5. Use UNIQUE Mode

Unless you need multiple hooks on same function:

```c
// Recommended for most cases
memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);
```

### 6. Logging

Use Android logging for debugging:

```c
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
```

---

## XDL Wrapper Examples

### Discover All Loaded Libraries

```c
typedef struct {
    const char* target;
    uintptr_t base;
} find_lib_ctx_t;

static bool find_library_callback(const MemKitLibInfo* info, void* user_data) {
    find_lib_ctx_t* ctx = (find_lib_ctx_t*)user_data;

    if (strcmp(info->name, ctx->target) == 0) {
        ctx->base = info->base;
        LOGI("Found %s at 0x%lx (size: %zu bytes)", 
             info->name, info->base, info->size);
        return false;  // Stop iteration
    }

    LOGD("Library: %s @ 0x%lx", info->name, info->base);
    return true;  // Continue
}

void discover_libraries() {
    find_lib_ctx_t ctx = {.target = "libil2cpp.so"};
    
    int count = memkit_xdl_iterate(find_library_callback, &ctx, XDL_DEFAULT);
    LOGI("Iterated %d libraries, found target at 0x%lx", count, ctx.base);
}
```

### Resolve Symbol from Any Library

```c
// Generic symbol resolution (not just IL2CPP)
void* resolve_from_libc() {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (!handle) return NULL;

    void* open_sym = memkit_xdl_sym(handle, "open", NULL);
    LOGI("libc.so::open = %p", open_sym);

    memkit_xdl_close(handle);
    return open_sym;
}

// One-shot with macro
void* sym = XDL_RESOLVE("libc.so", "open");
```

### Address-to-Symbol (Stack Trace / Debugging)

```c
void resolve_address(void* addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;

    if (memkit_xdl_addr_to_symbol(addr, &info, ctx)) {
        LOGI("Address %p:", addr);
        LOGI("  Library: %s (base: 0x%lx)", info.lib_name, info.lib_base);
        LOGI("  Symbol: %s (offset: 0x%lx, size: %zu)", 
             info.sym_name ? info.sym_name : "<unknown>",
             info.sym_offset, info.sym_size);
    } else {
        LOGI("Could not resolve address %p", addr);
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}

// Resolve multiple addresses (reuse context for performance)
void resolve_multiple_addresses(void** addrs, int count) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    
    for (int i = 0; i < count; i++) {
        MemKitSymInfo info;
        if (memkit_xdl_addr_to_symbol(addrs[i], &info, ctx)) {
            LOGI("[%d] %p -> %s!%s+0x%lx", 
                 i, addrs[i], info.lib_name, 
                 info.sym_name ? info.sym_name : "?", info.sym_offset);
        }
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}
```

### Get Library Information

```c
void print_lib_info(const char* lib_name) {
    void* handle = memkit_xdl_open(lib_name, XDL_DEFAULT);
    if (!handle) {
        LOGE("Could not open %s", lib_name);
        return;
    }

    MemKitLibInfo info;
    if (memkit_xdl_get_lib_info(handle, &info)) {
        LOGI("Library: %s", info.name);
        LOGI("  Base: 0x%lx", info.base);
        LOGI("  Path: %s", info.path ? info.path : "N/A");
    }

    memkit_xdl_close(handle);
}
```

### Fast Address-to-Library (Skip Symbol Lookup)

```c
// Use XDL_NON_SYM for faster lookup when you only need library info
void quick_lib_lookup(void* addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;

    // Skip symbol resolution for faster results
    if (memkit_xdl_addr_to_symbol4(addr, &info, ctx, XDL_NON_SYM)) {
        LOGI("Address %p is in %s (base: 0x%lx)", 
             addr, info.lib_name, info.lib_base);
        // info.sym_name will be NULL (skipped)
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}
```

### Thread Safety

The XDL wrapper is **thread-safe**:

```c
// Multiple threads can safely call memkit_xdl_iterate()
void* thread_func(void* arg) {
    // Each thread gets its own TLS buffer
    memkit_xdl_iterate(my_callback, NULL, XDL_DEFAULT);
    return NULL;
}

// Address resolution context is per-thread (NOT shared)
void* worker(void* arg) {
    // Create per-thread context
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    // ... use ctx ...
    memkit_xdl_addr_ctx_destroy(ctx);
    return NULL;
}
```

---

## Next Steps

- See [RECIPES.md](RECIPES.md) for common patterns
- See [SECURITY_RESEARCH.md](SECURITY_RESEARCH.md) for legitimate use cases
