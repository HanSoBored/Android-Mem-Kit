//
// Android-Mem-Kit: SLJIT JIT High-Level Wrappers
// Convenience functions built on SLJIT thin wrappers
//

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "memkit.h"
#include "sljitLir.h"

// ============================================================================
// TIER 2: HIGH-LEVEL WRAPPERS
// ============================================================================

// ----- Forwarding Trampoline ------------------------------------------------
//
// Generates a JIT-compiled function that forwards all W-sized arguments to
// a target function and returns its result. The generated code is
// architecture-independent and uses SLJIT's ABI-compatible calling convention.
//
// Args descriptor is built dynamically for 0-4 args, all as W (word).
// Uses scratch register passing (_R) so args are in R0..R{N-1} for icall.

void *memkit_jit_forwarder_create(void *target, int num_args) {
    if (!target || num_args < 0 || num_args > 4) {
        errno = EINVAL;
        return NULL;
    }

    sljit_s32 arg_types = SLJIT_ARG_RETURN(SLJIT_ARG_TYPE_W);
    for (int i = 1; i <= num_args; i++) {
        arg_types |= SLJIT_ARG_VALUE(SLJIT_ARG_TYPE_W_R, i);
    }

    sljit_s32 scratches = num_args > 2 ? num_args : 4;
    return memkit_jit_forwarder_create_explicit(target, arg_types, scratches);
}

void *memkit_jit_forwarder_create_explicit(void *target,
    sljit_s32 arg_types, sljit_s32 num_scratches) {
    if (!target) {
        errno = EINVAL;
        return NULL;
    }

    struct sljit_compiler *C = memkit_jit_create_compiler();
    if (!C) {
        errno = ENOMEM;
        return NULL;
    }

    sljit_s32 result = memkit_jit_emit_enter(C, 0, arg_types, num_scratches, 0, 0);
    if (result != SLJIT_SUCCESS) {
        memkit_jit_destroy_compiler(C);
        errno = EFAULT;
        return NULL;
    }

    result = memkit_jit_emit_icall(C, SLJIT_CALL, arg_types, SLJIT_IMM, (sljit_sw)target);
    if (result != SLJIT_SUCCESS) {
        memkit_jit_destroy_compiler(C);
        errno = EFAULT;
        return NULL;
    }

    result = memkit_jit_emit_return(C, SLJIT_MOV, SLJIT_RETURN_REG, 0);
    if (result != SLJIT_SUCCESS) {
        memkit_jit_destroy_compiler(C);
        errno = EFAULT;
        return NULL;
    }

    void *code = memkit_jit_generate_code(C);
    memkit_jit_destroy_compiler(C);

    if (!code) {
        errno = EFAULT;
        return NULL;
    }

    return code;
}

// ----- NOP Sled -------------------------------------------------------------

sljit_s32 memkit_jit_emit_nops(struct sljit_compiler *C, sljit_s32 nop_count) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    if (nop_count <= 0) return SLJIT_SUCCESS;

    for (sljit_s32 i = 0; i < nop_count; i++) {
        sljit_s32 result = memkit_jit_emit_op0(C, SLJIT_NOP);
        if (result != SLJIT_SUCCESS) {
            return result;
        }
    }
    return SLJIT_SUCCESS;
}

// ----- Executable Memory Allocation -----------------------------------------

#include <sys/mman.h>
#include <unistd.h>

void *memkit_jit_alloc_exec(size_t size) {
    if (size == 0) {
        errno = EINVAL;
        return NULL;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    size_t aligned = (size + (size_t)page_size - 1) & ~((size_t)page_size - 1);

    void *ptr = mmap(NULL, aligned, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }

    if (mprotect(ptr, aligned, PROT_READ | PROT_EXEC) != 0) {
        munmap(ptr, aligned);
        return NULL;
    }

    return ptr;
}

void memkit_jit_free_exec(void *ptr, size_t size) {
    if (!ptr || size == 0) return;

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    size_t aligned = (size + (size_t)page_size - 1) & ~((size_t)page_size - 1);
    munmap(ptr, aligned);
}

bool memkit_jit_write_exec(void *ptr, const void *data, size_t size) {
    if (!ptr || !data || size == 0) {
        errno = EINVAL;
        return false;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t aligned_start = start & ~((uintptr_t)page_size - 1);
    size_t aligned_size = ((start + size - aligned_start + (size_t)page_size - 1)
                          & ~((size_t)page_size - 1));

    if (mprotect((void*)aligned_start, aligned_size, PROT_READ | PROT_WRITE) != 0) {
        return false;
    }

    memcpy(ptr, data, size);

    if (mprotect((void*)aligned_start, aligned_size, PROT_READ | PROT_EXEC) != 0) {
        return false;
    }

#if defined(__arm__) || defined(__aarch64__)
    __builtin___clear_cache((char*)ptr, (char*)ptr + size);
#endif

    return true;
}
