# Android-Mem-Kit

**A Lightweight Native Instrumentation Library for Android Security Research**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Android 5.0+](https://img.shields.io/badge/Platform-Android%205.0+-blue.svg)]()
[![NDK: r25b+](https://img.shields.io/badge/NDK-r25b+-green.svg)]()

Android-Mem-Kit is a minimal-overhead, pure C library for Android native instrumentation. It provides memory patching, function hooking, and symbol resolution capabilities for **security research, debugging, and educational purposes**.

---

## ⚠️ Disclaimer

This library is intended for:
- ✅ **Security research** (analyzing app security, reverse engineering)
- ✅ **Educational purposes** (learning Android internals, hooking techniques)
- ✅ **Application debugging** (understanding native code behavior)
- ✅ **Malware analysis** (dynamic analysis of malicious apps)
- ✅ **Penetration testing** (with proper authorization)

**NOT intended for:**
- ❌ Game cheating or bypassing game protections
- ❌ Circumventing security in production applications
- ❌ Any illegal activities or unauthorized access

**Always use responsibly and within legal boundaries.**

---

## Features

| Feature | Implementation | Description |
| :--- | :--- | :--- |
| **Memory Patching** | Custom (mprotect-based) | Cross-page safe memory patching with XOM bypass |
| **Function Hooking** | [ShadowHook](https://github.com/bytedance/android-inline-hook) | ByteDance's inline hook library with excellent stability |
| **Symbol Resolution** | [XDL](https://github.com/hexhacking/xdl) | Advanced symbol resolution bypassing Android 7+ linker restrictions |
| **IL2CPP Support** | Built-in | Unity app analysis and instrumentation |

### Why Pure C?

- **Small Binary Size**: <100KB overhead (vs several MB for Rust)
- **Simple NDK Integration**: No FFI bridge or complex build setup
- **Direct JNI/NDK Access**: Native C integration with Android frameworks
- **Modern Tooling**: Leverages battle-tested libraries (ShadowHook, XDL)

---

## Quick Start

### 1. Prerequisites

```bash
# Android NDK (r25b or newer)
export ANDROID_NDK_HOME=/path/to/your/android-ndk-r29
```

### 2. Clone & Setup

```bash
git clone https://github.com/HanSoBored/Android-Mem-Kit.git
cd Android-Mem-Kit

# Clone dependencies (git submodules)
git submodule update --init --recursive
```

### 3. Build (Makefile - Recommended)

```bash
# Default build (arm64-v8a)
make

# Custom ABI
make ANDROID_ABI=armeabi-v7a

# Custom platform
make ANDROID_ABI=arm64-v8a ANDROID_PLATFORM=android-35

# Clean build
make clean && make
```

### 3. Build (CMake - Alternative)

```bash
mkdir build && cd build

# Configure for Android ARM64
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-35

# Build
cmake --build .
```

### 4. Basic Usage

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Original function pointer
static int (*orig_SSL_read)(void* ssl, void* buf, int num) = NULL;
static void* ssl_hook_stub = NULL;

// Hooked function - log SSL reads for research
static int my_SSL_read(void* ssl, void* buf, int num) {
    LOGI("SSL_read called with buffer: %p, size: %d", buf, num);

    // Call original
    int ret = orig_SSL_read(ssl, buf, num);

    if (ret > 0) {
        LOGI("Received %d bytes", ret);
        // Analyze decrypted data (for research only!)
    }

    return ret;
}

// Initialize when library loads
__attribute__((constructor))
void init() {
    // Initialize hooking
    if (memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Failed to init ShadowHook");
        return;
    }

    // Hook SSL_read (example for security research)
    ssl_hook_stub = memkit_hook_by_symbol(
        "libssl.so",
        "SSL_read",
        (void*)my_SSL_read,
        (void**)&orig_SSL_read
    );

    if (ssl_hook_stub) {
        LOGI("SSL_read hooked successfully!");
    }
}
```

---

## Documentation

For detailed usage and examples, see:

- **[docs/USAGE.md](docs/USAGE.md)** - Complete API reference and examples
- **[docs/RECIPES.md](docs/RECIPES.md)** - Common patterns and use cases
- **[docs/SECURITY_RESEARCH.md](docs/SECURITY_RESEARCH.md)** - Legitimate research examples

---

## Project Structure

```
Android-Mem-Kit/
├── CMakeLists.txt          # Build configuration (CMake)
├── Makefile                # Build configuration (Make - Recommended)
├── include/
│   └── memkit.h            # Public API header
├── src/
│   ├── memory.c            # Memory patching (mprotect-based)
│   ├── hooking.c           # ShadowHook wrapper
│   ├── il2cpp.c            # IL2CPP symbol resolution (uses memkit_xdl_* wrapper)
│   └── xdl_wrapper.c       # Generic xDL wrapper layer
├── examples/
│   └── main.c              # Complete usage example
├── docs/
│   ├── USAGE.md            # Detailed documentation
│   ├── RECIPES.md          # Common patterns
│   └── SECURITY_RESEARCH.md # Research use cases
└── deps/
    ├── xdl/                # XDL library (git submodule)
    └── shadowhook/         # ShadowHook library (git submodule)
```

---

## API Overview

### Memory Functions

```c
// Get library base address
uintptr_t base = memkit_get_lib_base("libtarget.so");

// Create patch from hex string
MemPatch* patch = memkit_patch_create(base + 0x1234, "00 00 80 D2");

// Apply/restore/free
memkit_patch_apply(patch);
memkit_patch_restore(patch);
memkit_patch_free(patch);
```

### Hooking Functions

```c
// Initialize (call once)
memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);

// Hook by symbol
void* stub = memkit_hook_by_symbol("lib.so", "func_name", my_func, (void**)&orig);

// Hook by address
void* stub = memkit_hook(address, my_func, (void**)&orig);

// Unhook
memkit_unhook(stub);
```

### IL2CPP Functions (Unity Apps)

```c
// Auto-cached function call
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");
void* domain = il2cpp_domain_get();

// Resolve from .symtab for internal symbols
void* internal = memkit_il2cpp_resolve_symtab("_ZN6Player13InternalInitEv");
```

---

## Common Use Cases

### 1. SSL Pinning Bypass (Research)

```c
// Hook SSL_verify_cert_chain to always return success
static int (*orig_SSL_verify_cert_chain)(void*) = NULL;

static int my_SSL_verify_cert_chain(void* cert_chain) {
    LOGI("SSL certificate verification intercepted");
    return 1; // Always succeed (for research only!)
}

memkit_hook_by_symbol("libssl.so", "SSL_verify_cert_chain",
                      my_SSL_verify_cert_chain, (void**)&orig_SSL_verify_cert_chain);
```

### 2. Integrity Check Bypass (Analysis)

```c
// Hook signature verification to return valid
static int (*orig_verifySignature)(const char* data) = NULL;

static int my_verifySignature(const char* data) {
    LOGI("Signature verification called with: %s", data);
    return 1; // Always valid (for analysis only!)
}

memkit_hook_by_symbol("libtarget.so", "verifySignature",
                      my_verifySignature, (void**)&orig_verifySignature);
```

### 3. Function Tracing (Debugging)

```c
// Trace all calls to a function
static void (*orig_targetFunc)(int param) = NULL;

static void my_targetFunc(int param) {
    LOGI("targetFunc called with param: %d", param);
    // Log stack trace, parameters, etc.
    orig_targetFunc(param);
}

memkit_hook_by_symbol("libtarget.so", "targetFunc",
                      my_targetFunc, (void**)&orig_targetFunc);
```

---

## ShadowHook Modes

| Mode | Description | Use Case |
| :--- | :--- | :--- |
| `SHADOWHOOK_MODE_UNIQUE` | Same address can only be hooked once | Most research scenarios |
| `SHADOWHOOK_MODE_SHARED` | Multiple hooks allowed (recursion prevention) | When using multiple SDKs |
| `SHADOWHOOK_MODE_MULTI` | Multiple hooks allowed (no prevention) | Advanced use cases |

---

## Troubleshooting

### Library Not Found

```c
// If memkit_get_lib_base() returns 0:
// 1. Ensure library is loaded in target process
// 2. Use exact name (e.g., "libil2cpp.so" not "il2cpp")
// 3. Add retry loop to wait for loading
uintptr_t base = 0;
for (int i = 0; i < 30 && base == 0; i++) {
    base = memkit_get_lib_base("libtarget.so");
    if (base == 0) sleep(1);
}
```

### Hook Fails

```c
void* stub = memkit_hook_by_symbol("lib.so", "func", my_func, (void**)&orig);
if (stub == NULL) {
    int err = shadowhook_get_errno();
    const char* msg = shadowhook_to_errmsg(err);
    LOGE("Hook failed: %d - %s", err, msg);
}
```

Common errors:
- `SHADOWHOOK_ERRNO_HOOK_DLSYM` - Symbol not found
- `SHADOWHOOK_ERRNO_HOOK_ENTER` - Failed to enter hook
- `SHADOWHOOK_ERRNO_UNINIT` - ShadowHook not initialized

---

## Migration from Rust Version

| Rust API | C Equivalent |
| :--- | :--- |
| `memory::get_lib_base()` | `memkit_get_lib_base()` |
| `MemoryPatch::from_hex()` | `memkit_patch_create()` |
| `hooking::attach()` | `memkit_hook()` / `memkit_hook_by_symbol()` |
| `il2cpp::resolve_export()` | `memkit_il2cpp_resolve()` |
| `il2cpp_call!()` | `IL2CPP_CALL()` |

---

## Credits

This project utilizes excellent open-source libraries:

- **[ShadowHook](https://github.com/bytedance/android-inline-hook)** by ByteDance - Inline hooking for Android
- **[XDL](https://github.com/hexhacking/xdl)** by HexHacking - Dynamic linker bypass
- **[Dobby](https://github.com/jmpews/Dobby)** - Lightweight hooking framework (original inspiration)
- **[KittyMemory](https://github.com/MJx0/KittyMemory)** - Memory patching library (original inspiration)

---

## License

MIT License - See [LICENSE](LICENSE) file for details.

---

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) first.

### Areas for Contribution:
- 📚 More documentation and examples
- 🔧 Additional utility functions
- 🐛 Bug fixes and improvements
- 🧪 Test cases for different Android versions

---

## Security Policy

If you find a security vulnerability, please see [SECURITY.md](SECURITY.md) for responsible disclosure guidelines.

---

## Support

- **Issues**: [GitHub Issues](https://github.com/HanSoBored/Android-Mem-Kit/issues)
- **Discussions**: [GitHub Discussions](https://github.com/HanSoBored/Android-Mem-Kit/discussions)

---

*Built for the security research community. Use responsibly.*
