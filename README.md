# android-mem-kit

Memory instrumentation library for Android (Rooted), written in Rust.
Includes Hooking (Dobby), Memory Patching (KittyMemory), and Address Resolution (XDL) features.

## Prerequisites

Since this library compiles C++ code natively, you **MUST** have the Android NDK.

1. Download Android NDK (r25+ recommended).
2. Set environment variables before build:

```bash
export ANDROID_NDK_HOME=/path/to/your/android-ndk-r29
```

## Usage

Add to `Cargo.toml`:

```toml
[dependencies]
android-mem-kit = "0.1.0"
```