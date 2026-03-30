//
// Android-Mem-Kit: xDL Wrapper Unit Tests
// Basic unit tests for xdl_wrapper module
//

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "memkit.h"
#include "xdl.h"

// ============================================================================
// Test Counters
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        g_tests_run++; \
        printf("[TEST] %s... ", #name); \
        fflush(stdout); \
        test_##name(); \
        g_tests_passed++; \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define RUN_TEST(name) run_test_##name()

// ============================================================================
// Test Callbacks
// ============================================================================

static bool dummy_callback(const MemKitLibInfo* info, void* user_data) {
    (void)info;
    (void)user_data;
    return true;
}

static bool find_libc_callback(const MemKitLibInfo* info, void* user_data) {
    bool* found = (bool*)user_data;
    if (info->name && strstr(info->name, "libc.so") != NULL) {
        *found = true;
        return false;  // Stop iteration
    }
    return true;  // Continue
}

// ============================================================================
// Test: memkit_xdl_iterate with NULL callback
// ============================================================================

TEST(memkit_xdl_iterate_null_callback) {
    int result = memkit_xdl_iterate(NULL, NULL, XDL_DEFAULT);
    assert(result == -1);
}

// ============================================================================
// Test: memkit_xdl_iterate with invalid flags
// ============================================================================

TEST(memkit_xdl_iterate_invalid_flags) {
    int result = memkit_xdl_iterate(dummy_callback, NULL, 0xFF);
    assert(result == -1);
}

// ============================================================================
// Test: memkit_xdl_open with NULL name
// ============================================================================

TEST(memkit_xdl_open_null_name) {
    void* handle = memkit_xdl_open(NULL, XDL_DEFAULT);
    assert(handle == NULL);
}

// ============================================================================
// Test: memkit_xdl_close with NULL handle
// ============================================================================

TEST(memkit_xdl_close_null_handle) {
    bool result = memkit_xdl_close(NULL);
    assert(result == false);
}

// ============================================================================
// Test: memkit_xdl_sym with NULL handle
// ============================================================================

TEST(memkit_xdl_sym_null_handle) {
    void* sym = memkit_xdl_sym(NULL, "open", NULL);
    assert(sym == NULL);
}

// ============================================================================
// Test: memkit_xdl_sym with NULL symbol
// ============================================================================

TEST(memkit_xdl_sym_null_symbol) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        void* sym = memkit_xdl_sym(handle, NULL, NULL);
        assert(sym == NULL);
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: memkit_xdl_dsym with NULL handle
// ============================================================================

TEST(memkit_xdl_dsym_null_handle) {
    void* sym = memkit_xdl_dsym(NULL, "open", NULL);
    assert(sym == NULL);
}

// ============================================================================
// Test: memkit_xdl_dsym with NULL symbol
// ============================================================================

TEST(memkit_xdl_dsym_null_symbol) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        void* sym = memkit_xdl_dsym(handle, NULL, NULL);
        assert(sym == NULL);
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: memkit_xdl_get_lib_info with NULL handle
// ============================================================================

TEST(memkit_xdl_get_lib_info_null_handle) {
    MemKitLibInfo info;
    bool result = memkit_xdl_get_lib_info(NULL, &info);
    assert(result == false);
}

// ============================================================================
// Test: memkit_xdl_get_lib_info with NULL output
// ============================================================================

TEST(memkit_xdl_get_lib_info_null_output) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        bool result = memkit_xdl_get_lib_info(handle, NULL);
        assert(result == false);
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: memkit_xdl_addr_ctx_create/destroy lifecycle
// ============================================================================

TEST(memkit_xdl_addr_ctx_lifecycle) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    assert(ctx != NULL);
    memkit_xdl_addr_ctx_destroy(ctx);
    // Should not crash when destroying NULL context twice
    memkit_xdl_addr_ctx_destroy(NULL);
}

// ============================================================================
// Test: memkit_xdl_addr_to_symbol with NULL address
// ============================================================================

TEST(memkit_xdl_addr_to_symbol_null_addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;
    bool result = memkit_xdl_addr_to_symbol(NULL, &info, ctx);
    assert(result == false);
    memkit_xdl_addr_ctx_destroy(ctx);
}

// ============================================================================
// Test: memkit_xdl_addr_to_symbol with NULL output
// ============================================================================

TEST(memkit_xdl_addr_to_symbol_null_output) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    bool result = memkit_xdl_addr_to_symbol((void*)0x1234, NULL, ctx);
    assert(result == false);
    memkit_xdl_addr_ctx_destroy(ctx);
}

// ============================================================================
// Test: memkit_xdl_addr_to_symbol4 with NULL address
// ============================================================================

TEST(memkit_xdl_addr_to_symbol4_null_addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;
    bool result = memkit_xdl_addr_to_symbol4(NULL, &info, ctx, XDL_DEFAULT);
    assert(result == false);
    memkit_xdl_addr_ctx_destroy(ctx);
}

// ============================================================================
// Test: memkit_xdl_addr_to_symbol4 with NULL output
// ============================================================================

TEST(memkit_xdl_addr_to_symbol4_null_output) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    bool result = memkit_xdl_addr_to_symbol4((void*)0x1234, NULL, ctx, XDL_DEFAULT);
    assert(result == false);
    memkit_xdl_addr_ctx_destroy(ctx);
}

// ============================================================================
// Test: memkit_xdl_open_from_phdr with NULL info
// ============================================================================

TEST(memkit_xdl_open_from_phdr_null_info) {
    void* handle = memkit_xdl_open_from_phdr(NULL);
    assert(handle == NULL);
}

// ============================================================================
// Test: Library iteration finds libc.so
// ============================================================================

TEST(memkit_xdl_iterate_finds_libc) {
    bool found = false;
    int count = memkit_xdl_iterate(find_libc_callback, &found, XDL_DEFAULT);
    assert(count >= 0);  // Should find at least some libraries
    assert(found == true);
}

// ============================================================================
// Test: Resolve symbol from libc.so
// ============================================================================

TEST(memkit_xdl_sym_resolves_open) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        void* sym = memkit_xdl_sym(handle, "open", NULL);
        assert(sym != NULL);
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: Resolve symbol with size output
// ============================================================================

TEST(memkit_xdl_sym_with_size) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        size_t size = 0;
        void* sym = memkit_xdl_sym(handle, "open", &size);
        assert(sym != NULL);
        // Size may be 0 for some symbols, but should not crash
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: Get library info for libc.so
// ============================================================================

TEST(memkit_xdl_get_lib_info_libc) {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (handle) {
        MemKitLibInfo info;
        bool result = memkit_xdl_get_lib_info(handle, &info);
        assert(result == true);
        assert(info.name != NULL);
        assert(info.base != 0);
        memkit_xdl_close(handle);
    }
}

// ============================================================================
// Test: Address-to-symbol with main() address
// ============================================================================

TEST(memkit_xdl_addr_to_symbol_main) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;
    
    // Try to resolve the address of this function
    bool result = memkit_xdl_addr_to_symbol((void*)&test_memkit_xdl_addr_to_symbol_main, &info, ctx);
    assert(result == true);
    assert(info.lib_name != NULL);
    assert(info.lib_base != 0);
    
    memkit_xdl_addr_ctx_destroy(ctx);
}

// ============================================================================
// Test: XDL_RESOLVE macro
// ============================================================================

TEST(xdl_resolve_macro) {
    void* sym = XDL_RESOLVE("libc.so", "open");
    assert(sym != NULL);
}

// ============================================================================
// Test: XDL_RESOLVE_SIZE macro
// ============================================================================

TEST(xdl_resolve_size_macro) {
    size_t size;
    void* sym = XDL_RESOLVE_SIZE("libc.so", "open", &size);
    assert(sym != NULL);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("===========================================\n");
    printf("  Android-Mem-Kit: xDL Wrapper Unit Tests\n");
    printf("===========================================\n\n");

    // NULL/Invalid Input Tests
    RUN_TEST(memkit_xdl_iterate_null_callback);
    RUN_TEST(memkit_xdl_iterate_invalid_flags);
    RUN_TEST(memkit_xdl_open_null_name);
    RUN_TEST(memkit_xdl_close_null_handle);
    RUN_TEST(memkit_xdl_sym_null_handle);
    RUN_TEST(memkit_xdl_sym_null_symbol);
    RUN_TEST(memkit_xdl_dsym_null_handle);
    RUN_TEST(memkit_xdl_dsym_null_symbol);
    RUN_TEST(memkit_xdl_get_lib_info_null_handle);
    RUN_TEST(memkit_xdl_get_lib_info_null_output);
    RUN_TEST(memkit_xdl_addr_ctx_lifecycle);
    RUN_TEST(memkit_xdl_addr_to_symbol_null_addr);
    RUN_TEST(memkit_xdl_addr_to_symbol_null_output);
    RUN_TEST(memkit_xdl_addr_to_symbol4_null_addr);
    RUN_TEST(memkit_xdl_addr_to_symbol4_null_output);
    RUN_TEST(memkit_xdl_open_from_phdr_null_info);

    // Functional Tests
    RUN_TEST(memkit_xdl_iterate_finds_libc);
    RUN_TEST(memkit_xdl_sym_resolves_open);
    RUN_TEST(memkit_xdl_sym_with_size);
    RUN_TEST(memkit_xdl_get_lib_info_libc);
    RUN_TEST(memkit_xdl_addr_to_symbol_main);

    // Macro Tests
    RUN_TEST(xdl_resolve_macro);
    RUN_TEST(xdl_resolve_size_macro);

    // Summary
    printf("\n===========================================\n");
    printf("  Test Summary\n");
    printf("===========================================\n");
    printf("  Run:     %d\n", g_tests_run);
    printf("  Passed:  %d\n", g_tests_passed);
    printf("  Failed:  %d\n", g_tests_failed);
    printf("===========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
