#include <stdarg.h>
#include <errno.h>

#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// INTERCEPT: BY FUNCTION ADDRESS
// ============================================================================

void *memkit_intercept(void *func_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...) {
    if (!func_addr || !pre) return NULL;

    void *stub;

    if (flags & MK_INTERCEPT_RECORD) {
        va_list ap;
        va_start(ap, flags);
        const char *record_lib_name = va_arg(ap, const char *);
        const char *record_sym_name = va_arg(ap, const char *);
        va_end(ap);
        stub = shadowhook_intercept_func_addr(func_addr, pre, data, flags, record_lib_name, record_sym_name);
    } else {
        stub = shadowhook_intercept_func_addr(func_addr, pre, data, flags);
    }

    return stub;
}

// ============================================================================
// INTERCEPT: BY SYMBOL ADDRESS
// ============================================================================

void *memkit_intercept_by_sym_addr(void *sym_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...) {
    if (!sym_addr || !pre) return NULL;

    void *stub;

    if (flags & MK_INTERCEPT_RECORD) {
        va_list ap;
        va_start(ap, flags);
        const char *record_lib_name = va_arg(ap, const char *);
        const char *record_sym_name = va_arg(ap, const char *);
        va_end(ap);
        stub = shadowhook_intercept_sym_addr(sym_addr, pre, data, flags, record_lib_name, record_sym_name);
    } else {
        stub = shadowhook_intercept_sym_addr(sym_addr, pre, data, flags);
    }

    return stub;
}

// ============================================================================
// INTERCEPT: BY SYMBOL NAME
// ============================================================================

void *memkit_intercept_by_symbol(const char *lib_name, const char *sym_name, MemKitInterceptor pre, void *data, uint32_t flags) {
    if (!lib_name || !sym_name || !pre) return NULL;
    void *stub = shadowhook_intercept_sym_name(lib_name, sym_name, pre, data, flags);
    if (!stub) {
        errno = shadowhook_get_errno();
    }
    return stub;
}

// ============================================================================
// INTERCEPT: AT INSTRUCTION ADDRESS
// ============================================================================

void *memkit_intercept_at_instr(void *instr_addr, MemKitInterceptor pre, void *data, uint32_t flags, ...) {
    if (!instr_addr || !pre) return NULL;

    void *stub;

    if (flags & MK_INTERCEPT_RECORD) {
        va_list ap;
        va_start(ap, flags);
        const char *record_lib_name = va_arg(ap, const char *);
        const char *record_sym_name = va_arg(ap, const char *);
        va_end(ap);
        stub = shadowhook_intercept_instr_addr(instr_addr, pre, data, flags, record_lib_name, record_sym_name);
    } else {
        stub = shadowhook_intercept_instr_addr(instr_addr, pre, data, flags);
    }

    return stub;
}

// ============================================================================
// INTERCEPT: WITH CALLBACK
// ============================================================================

void *memkit_intercept_with_callback(const char *lib_name, const char *sym_name, MemKitInterceptor pre, void *data, uint32_t flags, MemKitIntercepted intercepted, void *arg) {
    if (!lib_name || !sym_name || !pre) return NULL;
    void *stub = shadowhook_intercept_sym_name_callback(lib_name, sym_name, pre, data, flags, (shadowhook_intercepted_t)intercepted, arg);
    if (!stub) {
        errno = shadowhook_get_errno();
    }
    return stub;
}

// ============================================================================
// UNINTERCEPT
// ============================================================================

int memkit_unintercept(void *stub) {
    if (!stub) return MK_ERRNO_INVALID_ARG;
    return shadowhook_unintercept(stub);
}

// ============================================================================
// PROXY / STACK MANAGEMENT
// ============================================================================

void *memkit_get_prev_func(void *func) {
    return shadowhook_get_prev_func(func);
}

void memkit_pop_stack(const void *return_address) {
    shadowhook_pop_stack((void *)return_address);
}

void memkit_allow_reentrant(const void *return_address) {
    shadowhook_allow_reentrant((void *)return_address);
}

void memkit_disallow_reentrant(const void *return_address) {
    shadowhook_disallow_reentrant((void *)return_address);
}

void *memkit_get_return_address(void) {
    return shadowhook_get_return_address();
}
