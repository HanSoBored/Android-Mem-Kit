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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")

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
        file(GLOB XDL_SOURCES ${XDL_DIR}/src/main/cpp/*.c)
        add_library(xdl STATIC ${XDL_SOURCES})
        target_compile_definitions(xdl PRIVATE __ANDROID__API__=35)
        message(STATUS "Using local XDL: ${XDL_DIR}")
    else()
        message(WARNING "Local XDL not found. Run ./setup.sh or set USE_LOCAL_DEPS=OFF")
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
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/nothing/sh_nothing.c
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
        message(STATUS "Using local ShadowHook: ${SHADOWHOOK_DIR}")
    else()
        message(WARNING "Local ShadowHook not found. Run ./setup.sh or set USE_LOCAL_DEPS=OFF")
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
)

# Set library type based on MEMKIT_BUILD_SHARED option
if(MEMKIT_BUILD_SHARED)
    set_target_properties(memkit PROPERTIES LINKER_LANGUAGE C)
else()
    set_target_properties(memkit PROPERTIES LINKER_LANGUAGE C)
endif()

target_include_directories(memkit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${XDL_DIR}/include
    ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/include
)

target_link_libraries(memkit
    xdl
    shadowhook
    log
    android
)

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
