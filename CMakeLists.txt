cmake_minimum_required(VERSION 3.14)
project(memkit C ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# Subproject Detection
# ============================================================================

# Detect if memkit is being built as a subproject (via add_subdirectory)
# When CMAKE_SOURCE_DIR != CMAKE_CURRENT_SOURCE_DIR, we are a subproject.
set(MEMKIT_IS_SUBPROJECT OFF)
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MEMKIT_IS_SUBPROJECT ON)
endif()

# ============================================================================
# Android NDK Setup
# ============================================================================

if(DEFINED ENV{ANDROID_NDK_HOME})
    set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
elseif(DEFINED ENV{NDK_HOME})
    set(ANDROID_NDK $ENV{NDK_HOME})
endif()

# Toolchain detection helper: warn if NDK exists but no toolchain file set
# (This is too late to set CMAKE_TOOLCHAIN_FILE in subproject context)
if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{ANDROID_NDK_HOME})
    message(WARNING
        "ANDROID_NDK_HOME is set but CMAKE_TOOLCHAIN_FILE is not. "
        "If building for Android, ensure you pass "
        "-DCMAKE_TOOLCHAIN_FILE=\$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake "
        "to the top-level CMake invocation."
    )
endif()

if(NOT ANDROID_NDK AND NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "ANDROID_NDK_HOME or NDK_HOME environment variable must be set, or CMAKE_TOOLCHAIN_FILE must point to an Android toolchain file")
endif()

if(ANDROID_NDK)
    message(STATUS "Android NDK: ${ANDROID_NDK}")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wnull-dereference -Wformat=2 -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wimplicit-fallthrough -Wvla -Wno-macro-redefined -fdiagnostics-format=clang")

if(ENABLE_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    message(STATUS "AddressSanitizer: ENABLED")
endif()

if(ENABLE_UBSAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -g")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
    message(STATUS "UndefinedBehaviorSanitizer: ENABLED")
endif()
# -llog is linked via target_link_libraries(memkit log) below

# ============================================================================
# Build Options
# ============================================================================

# MEMKIT_BUILD_SHARED: When OFF (default for subprojects), build as STATIC library.
# When ON (default for standalone), build as SHARED library.
if(MEMKIT_IS_SUBPROJECT)
    option(MEMKIT_BUILD_SHARED "Build memkit as a shared library" OFF)
else()
    option(MEMKIT_BUILD_SHARED "Build memkit as a shared library" ON)
endif()

option(USE_LOCAL_DEPS "Use local dependencies from deps/ folder" ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# ============================================================================
# Include Directories
# ============================================================================

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# Dependencies
# ============================================================================

if(USE_LOCAL_DEPS)
    # Use local dependencies (faster, offline builds)

    # XDL library
    set(XDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/xdl/xdl)
    if(EXISTS ${XDL_DIR})
        include_directories(${XDL_DIR}/src/main/cpp/include)
        set(XDL_SOURCES
            ${XDL_DIR}/src/main/cpp/xdl.c
            ${XDL_DIR}/src/main/cpp/xdl_iterate.c
            ${XDL_DIR}/src/main/cpp/xdl_linker.c
            ${XDL_DIR}/src/main/cpp/xdl_lzma.c
            ${XDL_DIR}/src/main/cpp/xdl_util.c
        )
        add_library(xdl STATIC ${XDL_SOURCES})
        target_compile_definitions(xdl PRIVATE __ANDROID__API__=35)
        message(STATUS "Using local XDL: ${XDL_DIR}")
    else()
        message(WARNING "Local XDL not found. Clone deps via 'git submodule update --init --recursive' or set USE_LOCAL_DEPS=OFF")
    endif()

    # ShadowHook library
    # NOTE: ShadowHook v1.0.10+ is recommended for Android 15 compatibility.
    # See: https://github.com/bytedance/android-inline-hook/issues/91
    set(SHADOWHOOK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/shadowhook)
    if(EXISTS ${SHADOWHOOK_DIR})
        # Architecture auto-detection with proper ANDROID_ABI handling
        if(NOT ANDROID_ABI)
            if(CMAKE_ANDROID_ARCH_ABI)
                set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
            else()
                set(ANDROID_ABI "arm64-v8a")
            endif()
        endif()

        if(ANDROID_ABI STREQUAL "arm64-v8a")
            set(SH_ARCH_DIR "arm64")
        elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(SH_ARCH_DIR "arm")
        else()
            message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
        endif()

        include_directories(
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/include
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/arch/${SH_ARCH_DIR}
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/bsd
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/lss
        )
        set(SHADOWHOOK_SOURCES
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/arch/${SH_ARCH_DIR}/sh_a64.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/arch/${SH_ARCH_DIR}/sh_inst.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/arch/${SH_ARCH_DIR}/sh_glue.S
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/bytesig.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/sh_errno.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/sh_log.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/sh_ref.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/sh_trampo.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/common/sh_util.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_elf.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_enter.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_hub.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_island.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_jni.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_linker.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_recorder.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_safe.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_switch.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_task.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/sh_xdl.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/shadowhook.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl/xdl.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl/xdl_iterate.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl/xdl_linker.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl/xdl_lzma.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/xdl/xdl_util.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/third_party/bsd/queue.h
        )
        add_library(shadowhook STATIC ${SHADOWHOOK_SOURCES})
        target_compile_definitions(shadowhook PRIVATE __ANDROID__API__=35)
        
        # Build libshadowhook_nothing.so (required for Android 15+ compatibility)
        # Always built to generate architecture-correct embed header for the
        # auto-extract fallback, even when memkit itself is a static library.
        # NOTE: Keep flags in sync with Makefile's libshadowhook_nothing target
        add_library(shadowhook_nothing SHARED
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/nothing/sh_nothing.c)
        target_compile_options(shadowhook_nothing PRIVATE
            -Oz
            -Weverything
            -Werror
            -fno-ident
            -fno-unwind-tables
            -fno-asynchronous-unwind-tables
        )
        set(_shl
            "-Wl,--strip-all"
            "-Wl,--as-needed"
            "-Wl,--hash-style=sysv"
            "-Wl,--build-id=none"
            "-Wl,-nostdlib"
            "-nostartfiles"
            "-nodefaultlibs"
            "-Wl,-soname,libshadowhook_nothing.so"
        )
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            list(APPEND _shl "-Wl,-z,max-page-size=16384")
        endif()
        target_link_options(shadowhook_nothing PRIVATE ${_shl})
        unset(_shl)
        set_target_properties(shadowhook_nothing PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        )
        
        message(STATUS "Using local ShadowHook: ${SHADOWHOOK_DIR}")
    else()
        message(WARNING "Local ShadowHook not found. Clone deps via 'git submodule update --init --recursive' or set USE_LOCAL_DEPS=OFF")
    endif()

else()
    # Fetch from git (requires internet, slower first build)

    include(FetchContent)

    # XDL
    FetchContent_Declare(
        xdl
        GIT_REPOSITORY https://github.com/hexhacking/xdl.git
        GIT_TAG        v1.2.1
    )

    # ShadowHook
    # NOTE: v1.0.10+ recommended for Android 15 compatibility
    FetchContent_Declare(
        shadowhook
        GIT_REPOSITORY https://github.com/bytedance/android-inline-hook.git
        GIT_TAG        v1.3.0
    )

    message(STATUS "Fetching dependencies from git...")
    FetchContent_MakeAvailable(xdl shadowhook)

    set(SHADOWHOOK_DIR ${shadowhook_SOURCE_DIR})
    set(XDL_DIR ${xdl_SOURCE_DIR})
endif()

# ============================================================================
# Main memkit Library
# ============================================================================

# Determine library type based on MEMKIT_BUILD_SHARED option
if(MEMKIT_BUILD_SHARED)
    set(LIB_TYPE SHARED)
else()
    set(LIB_TYPE STATIC)
endif()

# Generate nothing_embed.h from the compiled libshadowhook_nothing.so
# Always generated when shadowhook_nothing target exists (local deps),
# ensuring the embed header matches the target architecture.
set(NOTHING_EMBED_H_PATH "${CMAKE_CURRENT_BINARY_DIR}/gen/nothing_embed.h")

if(TARGET shadowhook_nothing)
    add_custom_command(
        OUTPUT ${NOTHING_EMBED_H_PATH}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="$<TARGET_FILE:shadowhook_nothing>"
            -DOUTPUT_FILE=${NOTHING_EMBED_H_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_nothing_header.cmake
        DEPENDS shadowhook_nothing
        COMMENT "Generating nothing_embed.h from libshadowhook_nothing.so"
    )
    add_custom_target(generate_nothing_embed DEPENDS ${NOTHING_EMBED_H_PATH})
else()
    # Fallback (when shadowhook_nothing target isn't available, e.g. FetchContent path):
    # use pre-generated header from source tree.
    message(WARNING
        "shadowhook_nothing target not found; using pre-generated "
        "arm64-v8a embed header. Set USE_LOCAL_DEPS=ON for multi-ABI support."
    )
    set(NOTHING_EMBED_H_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/nothing_embed.h")
endif()

add_library(memkit ${LIB_TYPE}
    src/memory.c
    src/hooking.c
    src/hooking_flags.c
    src/intercept.c
    src/records.c
    src/runtime_config.c
    src/dl_callbacks.c
    src/il2cpp.c
    src/il2cpp_safe.c
    src/xdl_wrapper.c
    src/nothing_path.c
    src/shadowhook_override.c
)

# Add dependency on generated header
if(TARGET generate_nothing_embed)
    add_dependencies(memkit generate_nothing_embed)
endif()

# Set library type based on MEMKIT_BUILD_SHARED option
set_target_properties(memkit PROPERTIES LINKER_LANGUAGE C)

target_include_directories(memkit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include          # memkit.h (public API)
)
if(DEFINED SHADOWHOOK_DIR)
    # shadowhook.h is PUBLIC because memkit.h includes it
    target_include_directories(memkit PUBLIC
        ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/include
    )
endif()
target_include_directories(memkit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${XDL_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/gen
)

target_link_libraries(memkit
    xdl
    shadowhook
    log
    android
)

# --wrap flags: intercept dlopen and sh_linker_init at link time
if(MEMKIT_BUILD_SHARED)
    target_link_options(memkit PRIVATE
        "LINKER:--wrap,dlopen"
        "LINKER:--wrap,sh_linker_init"
    )
else()
    # NOTE: When memkit is built as a STATIC library, the --wrap flags are
    # propagated via INTERFACE. This means any consumer linking against
    # libmemkit.a will also have dlopen and sh_linker_init wrapped.
    #
    # This is intentional — it ensures __wrap_dlopen intercepts the
    # libshadowhook_nothing.so dlopen() call even when memkit is statically
    # linked into a larger shared library.
    #
    # LIMITATION: If the consumer already wraps dlopen for other purposes,
    # this may cause linker conflicts. In that case, consider building memkit
    # as a SHARED library (MEMKIT_BUILD_SHARED=ON) where the flags are PRIVATE.
    target_link_options(memkit INTERFACE
        "LINKER:--wrap,dlopen"
        "LINKER:--wrap,sh_linker_init"
    )
endif()

set_target_properties(memkit PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
)

# ============================================================================
# Example (Optional)
# ============================================================================

option(BUILD_EXAMPLES "Build example projects" OFF)

if(BUILD_EXAMPLES)
    add_library(memkit-example SHARED
        examples/main.c
    )
    target_link_libraries(memkit-example memkit)
    set_target_properties(memkit-example PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/example"
    )
    message(STATUS "Building examples")
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS memkit
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/memkit.h
    DESTINATION include
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "  Android NDK: ${ANDROID_NDK}")
message(STATUS "  Subproject:  ${MEMKIT_IS_SUBPROJECT}")
message(STATUS "  Shared lib:  ${MEMKIT_BUILD_SHARED}")
message(STATUS "  Local deps:  ${USE_LOCAL_DEPS}")
message(STATUS "  Examples:    ${BUILD_EXAMPLES}")
