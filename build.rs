// build.rs
use std::env;
use std::path::PathBuf;

fn main() {
    if std::env::var("DOCS_RS").is_ok() {
        return;
    }
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    if target_os != "android" {
        println!("cargo:warning=android-mem-kit is designed for Android targets.");
        return;
    }

    // --- 1. Build XDL (C Library) ---
    // Xdl bypasses linker restrictions. Essential for Android 7+.
    cc::Build::new()
        .file("deps/xdl/xdl/src/main/cpp/xdl.c")
        .include("deps/xdl/xdl/src/main/cpp/include")
        .compile("xdl");
    println!("cargo:rustc-link-lib=static=xdl");

    // --- 2. Build KittyMemory (C++ Library) ---
    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .file("deps/KittyMemory/KittyMemory/KittyMemory.cpp")
        .file("deps/KittyMemory/KittyMemory/MemoryPatch.cpp")
        .file("deps/KittyMemory/KittyMemory/KittyUtils.cpp")
        .include("deps/KittyMemory/KittyMemory")
        .compile("kittymemory");
    println!("cargo:rustc-link-lib=static=kittymemory");

    // --- 3. Build Dobby (Hooking) ---
    let ndk_home = env::var("ANDROID_NDK_HOME").expect("Error: Environment variable ANDROID_NDK_HOME belum diset!");
    let toolchain_path = PathBuf::from(&ndk_home)
        .join("build/cmake/android.toolchain.cmake");

    if !toolchain_path.exists() {
        panic!("Toolchain file tidak ditemukan di: {}", toolchain_path.display());
    }

    let dst = cmake::Config::new("deps/Dobby")
        .define("DOBBY_DEBUG", "OFF")
        .define("CMAKE_TOOLCHAIN_FILE", toolchain_path)
        .define("ANDROID_PLATFORM", "android-24")
        .define("ANDROID_ABI", "arm64-v8a")
        .define("BUILD_SHARED_LIBS", "OFF") 
        .build_target("dobby_static") 
        .build();
    
    println!("cargo:rustc-link-search=native={}/build", dst.display());
    
    // Link library
    println!("cargo:rustc-link-lib=static=dobby");

    // --- 4. Build Internal Bridge ---
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let kittymemory_include = manifest_dir.join("deps/KittyMemory/KittyMemory");
    let xdl_include = manifest_dir.join("deps/xdl/xdl/src/main/cpp/include");

    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .file("src/bridge/bridge.cpp")
        .include(kittymemory_include)
        .include(xdl_include)
        .compile("memkit_bridge");

    println!("cargo:rustc-link-lib=static=memkit_bridge");
}