/**
 * Android-Mem-Kit Example - Security Research Instrumentation
 *
 * This example demonstrates legitimate security research use cases:
 * 1. SSL/TLS analysis for vulnerability assessment
 * 2. Integrity check analysis
 * 3. Function tracing for behavior analysis
 * 4. IL2CPP instrumentation for Unity app analysis
 *
 * IMPORTANT: Only use on apps you have authorization to test!
 *
 * Build: Use CMake with Android NDK
 * Usage: Load this library in your test/research environment
 */

#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// LOGGING MACRO
// ============================================================================

#define LOG_TAG "MemKit-Research"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ============================================================================
// EXAMPLE 1: SSL/TLS Certificate Analysis (Security Research)
// ============================================================================
// Use case: Analyze how apps handle SSL certificate validation
// Only use on apps you have authorization to test!

static int (*orig_SSL_verify_cert_chain)(void* ssl) = NULL;
static void* ssl_verify_stub = NULL;

/**
 * Hooked SSL_verify_cert_chain - Analyze certificate validation behavior
 * 
 * Security Research Goal: Understand if the app properly validates certificates
 */
static int mod_SSL_verify_cert_chain(void* ssl) {
    LOGI("[SSL Research] Certificate verification called");
    
    // Log the verification attempt (for analysis)
    LOGD("[SSL Research] SSL context: %p", ssl);
    
    // Call original to observe behavior
    int result = orig_SSL_verify_cert_chain(ssl);
    
    LOGI("[SSL Research] Verification result: %s", 
         result ? "SUCCESS (cert valid)" : "FAILED (cert rejected)");
    
    return result;
}

// ============================================================================
// EXAMPLE 2: Integrity Check Analysis
// ============================================================================
// Use case: Analyze app integrity verification mechanisms

static int (*orig_verifySignature)(const char* data, const char* signature) = NULL;
static void* signature_stub = NULL;

/**
 * Hooked signature verification - Analyze integrity checks
 * 
 * Security Research Goal: Document how the app verifies its integrity
 */
static int mod_verifySignature(const char* data, const char* signature) {
    LOGI("[Integrity Research] Signature verification called");
    
    // Log verification attempt (truncated for readability)
    if (data) {
        LOGD("[Integrity Research] Data preview: %.50s...", data);
    }
    if (signature) {
        LOGD("[Integrity Research] Signature: %.32s...", signature);
    }
    
    // Call original and log result
    int result = orig_verifySignature(data, signature);
    
    LOGI("[Integrity Research] Signature check: %s", 
         result ? "VALID" : "INVALID");
    
    return result;
}

// ============================================================================
// EXAMPLE 3: Function Tracing (Behavior Analysis)
// ============================================================================
// Use case: Trace function calls to understand app behavior

static void (*orig_targetFunction)(int param1, void* param2) = NULL;
static void* trace_stub = NULL;

/**
 * Hooked function - Trace calls for behavior analysis
 * 
 * Security Research Goal: Understand function call patterns
 */
static void mod_targetFunction(int param1, void* param2) {
    LOGI("[Trace Research] targetFunction called:");
    LOGD("  param1 (int): %d", param1);
    LOGD("  param2 (ptr): %p", param2);
    
    // Call original
    orig_targetFunction(param1, param2);
    
    LOGD("[Trace Research] targetFunction returned");
}

// ============================================================================
// EXAMPLE 4: IL2CPP Analysis (Unity App Research)
// ============================================================================
// Use case: Analyze Unity app native code behavior

static void* (*orig_il2cpp_domain_get)(void) = NULL;
static void* (*orig_il2cpp_thread_attach)(void*) = NULL;

// ============================================================================
// MAIN RESEARCH THREAD
// ============================================================================

static void* research_thread(void* arg) {
    (void)arg;

    LOGI("=== Android-Mem-Kit Research Example Starting ===");
    LOGI("IMPORTANT: Only use on apps you have authorization to test!");

    // -------------------------------------------------------------------------
    // Step 1: Wait for target library to load
    // -------------------------------------------------------------------------
    LOGI("Waiting for libil2cpp.so to load...");

    uintptr_t il2cpp_base = 0;
    int retries = 30;

    while (retries > 0) {
        il2cpp_base = memkit_get_lib_base("libil2cpp.so");
        if (il2cpp_base != 0) {
            break;
        }
        sleep(1);
        retries--;
    }

    if (il2cpp_base == 0) {
        LOGE("libil2cpp.so not found after 30 seconds");
        LOGI("This is expected if analyzing non-Unity apps");
    } else {
        LOGI("libil2cpp.so found at base: 0x%" PRIxPTR, il2cpp_base);
    }

    // -------------------------------------------------------------------------
    // Step 2: Initialize ShadowHook
    // -------------------------------------------------------------------------
    int hook_init_result = memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);
    if (hook_init_result != 0) {
        int err = memkit_errno();
        const char* msg = memkit_strerror(err);
        LOGE("Failed to initialize ShadowHook: %d - %s", err, msg);
        return NULL;
    }
    LOGI("ShadowHook initialized (UNIQUE mode)");

    // -------------------------------------------------------------------------
    // Step 3: SSL/TLS Analysis Hook (Research Example)
    // -------------------------------------------------------------------------
    LOGI("Setting up SSL verification analysis...");
    
    ssl_verify_stub = memkit_hook_by_symbol(
        "libssl.so",
        "SSL_verify_cert_chain",
        (void*)mod_SSL_verify_cert_chain,
        (void**)&orig_SSL_verify_cert_chain
    );

    if (ssl_verify_stub) {
        LOGI("SSL verification hook installed (for analysis)");
    } else {
        int err = memkit_errno();
        LOGD("SSL hook not installed (app may not use libssl.so): %d", err);
    }

    // -------------------------------------------------------------------------
    // Step 4: Integrity Check Analysis Hook
    // -------------------------------------------------------------------------
    LOGI("Setting up integrity check analysis...");
    
    signature_stub = memkit_hook_by_symbol(
        "libtarget.so",
        "verifySignature",
        (void*)mod_verifySignature,
        (void**)&orig_verifySignature
    );

    if (signature_stub) {
        LOGI("Signature verification hook installed (for analysis)");
    } else {
        int err = memkit_errno();
        LOGD("Signature hook not installed (symbol may not exist): %d", err);
    }

    // -------------------------------------------------------------------------
    // Step 5: Function Tracing Hook
    // -------------------------------------------------------------------------
    LOGI("Setting up function tracing...");
    
    // Note: Replace with actual function address from your research target
    // uintptr_t target_func_addr = il2cpp_base + 0xXXXXXX;
    // trace_stub = memkit_hook(
    //     target_func_addr,
    //     (void*)mod_targetFunction,
    //     (void**)&orig_targetFunction
    // );

    // -------------------------------------------------------------------------
    // Step 6: IL2CPP API Analysis
    // -------------------------------------------------------------------------
    if (il2cpp_base != 0) {
        LOGI("Attempting IL2CPP API analysis...");

        // Get IL2CPP Domain (auto-cached via macro)
        void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");
        if (il2cpp_domain_get) {
            void* domain = il2cpp_domain_get();
            if (domain) {
                LOGI("IL2CPP Domain obtained: %p", domain);

                // Attach thread
                void* (*il2cpp_thread_attach)(void*) = 
                    IL2CPP_CALL(void*, "il2cpp_thread_attach", void*);
                if (il2cpp_thread_attach) {
                    void* result = il2cpp_thread_attach(domain);
                    LOGI("Thread attached to IL2CPP domain: %p", result);
                }
            }
        }

        // Get IL2CPP Root Namespace (for Unity class analysis)
        void* (*il2cpp_get_root_namespace)(void) = 
            IL2CPP_CALL(void*, "il2cpp_get_root_namespace");
        if (il2cpp_get_root_namespace) {
            void* root_ns = il2cpp_get_root_namespace();
            LOGI("IL2CPP Root Namespace: %p", root_ns);
        }

        // Example: Class lookup (replace with actual class from your research)
        void* (*il2cpp_class_from_name)(const char*, const char*, const char*) =
            IL2CPP_CALL(void*, "il2cpp_class_from_name", 
                       const char*, const char*, const char*);

        if (il2cpp_class_from_name) {
            // Replace with actual class from your research target
            // void* some_class = il2cpp_class_from_name("Assembly-CSharp", "", "SomeClass");
            LOGD("IL2CPP class lookup API available");
        }

        // Example: Resolve internal symbol from .symtab
        void* internal_func = memkit_il2cpp_resolve_symtab("_ZN6Player13InternalInitEv");
        if (internal_func) {
            LOGI("Found internal function via .symtab: %p", internal_func);
        } else {
            LOGD("Internal symbol not found (expected for most apps)");
        }
    }

    // -------------------------------------------------------------------------
    // Step 7: Memory Patching Example (Safe, Reversible)
    // -------------------------------------------------------------------------
    if (il2cpp_base != 0) {
        LOGI("Demonstrating memory patching (research only)...");
        
        // Example: Create a NOP patch (ARM64: NOP)
        // Replace with actual address from your research
        // uintptr_t patch_addr = il2cpp_base + 0xXXXXXX;
        // MemPatch* test_patch = memkit_patch_create(patch_addr, "1F 20 03 D5");
        
        // if (test_patch && memkit_patch_apply(test_patch)) {
        //     LOGI("Test patch applied (for research)");
        //     
        //     // Restore immediately (demonstration only)
        //     memkit_patch_restore(test_patch);
        //     LOGI("Test patch restored");
        //     
        //     memkit_patch_free(test_patch);
        // }
        
        LOGI("Memory patching API ready for use");
    }

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    LOGI("=== Research Setup Complete ===");
    LOGI("Active Analysis Hooks:");
    if (ssl_verify_stub) {
        LOGI("  - SSL certificate verification (analysis)");
    }
    if (signature_stub) {
        LOGI("  - Signature verification (analysis)");
    }
    if (trace_stub) {
        LOGI("  - Function tracing (analysis)");
    }
    
    LOGI("");
    LOGI(" Research Guidelines ");
    LOGI("1. Only analyze apps you have authorization to test");
    LOGI("2. Document findings responsibly");
    LOGI("3. Follow responsible disclosure if vulnerabilities found");
    LOGI("4. Respect user privacy and data");

    return NULL;
}

// ============================================================================
// CLEANUP FUNCTION (Call when unloading)
// ============================================================================

static void cleanup_all(void) {
    LOGI("Cleaning up research hooks...");

    // Unhook SSL
    if (ssl_verify_stub) {
        memkit_unhook(ssl_verify_stub);
        LOGI("Unhooked SSL_verify_cert_chain");
    }

    // Unhook signature verification
    if (signature_stub) {
        memkit_unhook(signature_stub);
        LOGI("Unhooked verifySignature");
    }

    // Unhook tracer
    if (trace_stub) {
        memkit_unhook(trace_stub);
        LOGI("Unhooked targetFunction");
    }

    LOGI("Cleanup complete");
}

// ============================================================================
// LIBRARY CONSTRUCTOR (AUTO-RUN ON LOAD)
// ============================================================================

__attribute__((constructor, visibility("default")))
static void on_library_load(void) {
    LOGI("MemKit research library loaded");
    LOGI("Purpose: Security research and behavior analysis");

    // Create detached thread (won't block main thread)
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int result = pthread_create(&thread, &attr, research_thread, NULL);
    if (result != 0) {
        LOGE("Failed to create research thread: %d", result);
    }

    pthread_attr_destroy(&attr);
}

// ============================================================================
// JNI ONLOAD (Optional - For Java integration)
// ============================================================================

#ifdef JNI_VERSION_1_6
#include <jni.h>

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    LOGI("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}

// Optional: Export cleanup function for Java to call
JNIEXPORT void JNICALL 
Java_com_example_research_MemKit_cleanup(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    cleanup_all();
}
#endif
