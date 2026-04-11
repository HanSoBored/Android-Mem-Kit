# Migration from Rust Version

This guide maps the original Rust APIs to their C equivalents in Android-Mem-Kit.

## Function Mapping

| Rust API | C Equivalent |
| :--- | :--- |
| `memory::get_lib_base()` | `memkit_get_lib_base()` |
| `MemoryPatch::from_hex()` | `memkit_patch_create()` |
| `MemoryPatch::patch()` | `memkit_patch_apply()` |
| `MemoryPatch::restore()` | `memkit_patch_restore()` |
| `hooking::attach()` | `memkit_hook()` / `memkit_hook_by_symbol()` |
| `hooking::detach()` | `memkit_unhook()` |
| `il2cpp::resolve_export()` | `memkit_il2cpp_resolve()` |
| `il2cpp::resolve_symtab()` | `memkit_il2cpp_resolve_symtab()` |
| `il2cpp_call!()` | `IL2CPP_CALL()` |

## Key Differences

### 1. Error Handling

**Rust:** `Result<T, Error>` with pattern matching.

```rust
match memkit_hook_by_symbol(...) {
    Ok(stub) => println!("Hooked"),
    Err(e) => eprintln!("Failed: {}", e),
}
```

**C:** Returns `NULL` on failure, sets `errno`. Use `memkit_errno()` and `memkit_strerror()` for ShadowHook-specific codes.

```c
void* stub = memkit_hook_by_symbol(...);
if (!stub) {
    int err = memkit_errno();
    fprintf(stderr, "Hook failed: %d - %s\n", err, memkit_strerror(err));
}
```

### 2. Memory Management

**Rust:** Automatic via `Drop` trait.

```rust
let patch = MemoryPatch::from_hex(...);
// Automatically freed when `patch` goes out of scope
```

**C:** Manual management required.

```c
MemPatch* patch = memkit_patch_create(...);
memkit_patch_apply(patch);
// ...
memkit_patch_restore(patch);
memkit_patch_free(patch);  // Must call explicitly
```

### 3. Hook Initialization

**Rust:** Implicit initialization on first hook call.

```rust
hooking::attach(target, replacement, &original);
```

**C:** Explicit initialization required before any hook call.

```c
memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);  // Call once, before any hooks
void* stub = memkit_hook_by_symbol(target, replacement, (void**)&original);
```

### 4. IL2CPP Calls

**Rust:** Macro with type inference.

```rust
let domain = il2cpp_call!(void*, "il2cpp_domain_get");
```

**C:** Macro with explicit return type.

```c
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");
void* domain = il2cpp_domain_get();
```

---

## What's New (C Version Has That Rust Didn't)

The C wrapper exposes significantly more of ShadowHook's API:

| Feature | C API | Description |
|---------|-------|-------------|
| **Intercept API** | `memkit_intercept_*` | Pre-call CPU context inspection without replacing the function |
| **Proxy Chaining** | `MEMKIT_CALL_PREV()`, `MEMKIT_POP_STACK()` | MULTI mode proxy chaining |
| **Records API** | `memkit_get_records()`, `memkit_dump_records_fd()` | Operation audit logging |
| **Runtime Config** | `memkit_set_debuggable()`, `memkit_set_recordable()` | Toggle settings at runtime |
| **V2 Hook API** | `memkit_hook_v2()` | Per-hook mode flags (override global mode) |
| **DL Callbacks** | `memkit_register_dl_*_callback()` | Auto-detect library load/unload events |
| **IL2CPP Helpers** | `memkit_il2cpp_get_image()`, `memkit_il2cpp_safe_call()`, `memkit_il2cpp_wait_ready()`, `memkit_il2cpp_attach_thread()`, `memkit_il2cpp_detach_thread()` | Runtime-safe IL2CPP instrumentation with crash protection |

See [USAGE.md](USAGE.md) for full API reference.
