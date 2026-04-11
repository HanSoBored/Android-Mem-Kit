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
| **Function Hooking** | [ShadowHook](https://github.com/bytedance/android-inline-hook) | Inline hook with intercept, proxy chaining, and records |
| **Symbol Resolution** | [XDL](https://github.com/hexhacking/xdl) | Bypasses Android 7+ linker restrictions |
| **IL2CPP Support** | Built-in | Unity app analysis and instrumentation |

### Why Pure C?

- **Small Binary Size**: <100KB overhead
- **Simple NDK Integration**: No FFI bridge or complex build setup
- **Direct JNI/NDK Access**: Native C integration with Android frameworks
- **Modern Tooling**: Leverages battle-tested libraries (ShadowHook, XDL)

---

## Quick Start

### 1. Prerequisites

```bash
export ANDROID_NDK_HOME=/path/to/your/android-ndk-r29
```

### 2. Clone & Setup

```bash
git clone https://github.com/HanSoBored/Android-Mem-Kit.git
cd Android-Mem-Kit
git submodule update --init --recursive
```

### 3. Build

```bash
# Default build (arm64-v8a)
make

# Custom ABI
make ANDROID_ABI=armeabi-v7a

# Clean build
make clean && make

# Custom build with CMake (for subproject use)
cmake -B build -DMEMKIT_BUILD_SHARED=OFF -DCMAKE_TOOLCHAIN_FILE=...
cmake --build build
```

### 4. Basic Usage

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static int (*orig_SSL_read)(void* ssl, void* buf, int num) = NULL;

static int my_SSL_read(void* ssl, void* buf, int num) {
    LOGI("SSL_read: buf=%p, size=%d", buf, num);
    return orig_SSL_read(ssl, buf, num);
}

__attribute__((constructor))
void init() {
    memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);

    void* stub = memkit_hook_by_symbol(
        "libssl.so", "SSL_read",
        (void*)my_SSL_read,
        (void**)&orig_SSL_read
    );

    if (stub) LOGI("SSL_read hooked!");
}
```

---

## Documentation

| Document | Content |
|----------|---------|
| **[docs/USAGE.md](docs/USAGE.md)** | Complete API reference, intercept API, records, runtime config, DL callbacks, troubleshooting |
| **[docs/RECIPES.md](docs/RECIPES.md)** | Common patterns: SSL pinning bypass, integrity checks, function tracing |
| **[docs/SECURITY_RESEARCH.md](docs/SECURITY_RESEARCH.md)** | Legitimate research examples |
| **[docs/MIGRATION.md](docs/MIGRATION.md)** | Migration guide from the Rust version |

---

## Project Structure

```
Android-Mem-Kit/
├── include/
│   └── memkit.h            # Public API header (~500 lines)
├── src/
│   ├── memory.c            # Memory patching (mprotect-based)
│   ├── hooking.c           # Basic hook/unhook + error handling
│   ├── hooking_flags.c     # V2 hook API with mode flags
│   ├── intercept.c         # Intercept API (pre-call CPU context inspection)
│   ├── records.c           # Records API (operation logging)
│   ├── runtime_config.c    # Runtime configuration
│   ├── dl_callbacks.c      # DL init/fini callbacks
│   ├── il2cpp.c            # IL2CPP symbol resolution
│   ├── il2cpp_safe.c       # IL2CPP safe call helpers (sigsetjmp/siglongjmp)
│   └── xdl_wrapper.c       # xDL wrapper layer
├── examples/
│   └── main.c              # Complete usage example
├── docs/                   # Documentation
└── deps/                   # Submodules: shadowhook, xdl
```

---

## Known Issues

### ShadowHook Error 12 on Android 15

When using ShadowHook on **Android 15 (API 35)**, you may encounter **error code 12** (`MK_ERRNO_INIT_LINKER`) during `memkit_hook_init()`. This is caused by changes to Android's internal linker behavior in Android 15 that affect ShadowHook's hooking mechanism.

**Affected versions:** ShadowHook < v1.0.10

**Workaround:**
- Pin ShadowHook to **v1.0.10 or later** which includes Android 15 compatibility fixes.
- If using local dependencies, update the submodule:
  ```bash
  cd deps/shadowhook
  git checkout v1.0.10  # or later
  ```
- If fetching via git (`USE_LOCAL_DEPS=OFF`), the CMakeLists.txt already fetches v1.3.0 which includes the fix.

**Tracking:** See upstream issue [bytedance/android-inline-hook#91](https://github.com/bytedance/android-inline-hook/issues/91) for the latest status.

### Subproject Builds

When building memkit as a subproject (`add_subdirectory`), ensure you set `-DMEMKIT_BUILD_SHARED=OFF` to build a static library. The default is `OFF` for subprojects and `ON` for standalone builds.

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

---

## Support

- **Issues**: [GitHub Issues](https://github.com/HanSoBored/Android-Mem-Kit/issues)
- **Discussions**: [GitHub Discussions](https://github.com/HanSoBored/Android-Mem-Kit/discussions)

---

*Built for the security research community. Use responsibly.*
