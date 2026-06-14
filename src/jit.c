//
// Android-Mem-Kit: SLJIT JIT Compiler Wrapper
// Platform-independent JIT code generation for Android security research
//

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "memkit.h"
#include "sljitLir.h"

// ============================================================================
// TIER 1: THIN WRAPPERS (1:1 mapping to SLJIT functions)
// ============================================================================

// ----- Lifecycle -----------------------------------------------------------

struct sljit_compiler *memkit_jit_create_compiler(void) {
    return sljit_create_compiler(NULL);
}

void memkit_jit_destroy_compiler(struct sljit_compiler *C) {
    if (C) {
        sljit_free_compiler(C);
    }
}

sljit_s32 memkit_jit_get_error(struct sljit_compiler *C) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_get_compiler_error(C);
}

void *memkit_jit_generate_code(struct sljit_compiler *C) {
    if (!C) {
        errno = EINVAL;
        return NULL;
    }

    void *code = sljit_generate_code(C, 0, NULL);
    if (!code) {
        errno = EFAULT;
        return NULL;
    }

    return code;
}

void memkit_jit_free_code(void *code) {
    if (code) {
        sljit_free_code(code, NULL);
    }
}

sljit_s32 memkit_jit_has_cpu_feature(sljit_s32 feature_type) {
    return sljit_has_cpu_feature(feature_type);
}

const char *memkit_jit_get_platform_name(void) {
    return sljit_get_platform_name();
}

// ----- Function Entry/Exit -------------------------------------------------

sljit_s32 memkit_jit_emit_enter(struct sljit_compiler *C, sljit_s32 options,
    sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds, sljit_s32 local_size) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_enter(C, options, arg_types, scratches, saveds, local_size);
}

sljit_s32 memkit_jit_set_context(struct sljit_compiler *C, sljit_s32 options,
    sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds, sljit_s32 local_size) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_set_context(C, options, arg_types, scratches, saveds, local_size);
}

sljit_s32 memkit_jit_emit_return_void(struct sljit_compiler *C) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_return_void(C);
}

sljit_s32 memkit_jit_emit_return(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_return(C, op, src, srcw);
}

sljit_s32 memkit_jit_emit_return_to(struct sljit_compiler *C,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_return_to(C, src, srcw);
}

// ----- Instructions: Zero Operand ------------------------------------------

sljit_s32 memkit_jit_emit_op0(struct sljit_compiler *C, sljit_s32 op) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op0(C, op);
}

// ----- Instructions: One Operand -------------------------------------------

sljit_s32 memkit_jit_emit_op1(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op1(C, op, dst, dstw, src, srcw);
}

// ----- Instructions: Two Operands ------------------------------------------

sljit_s32 memkit_jit_emit_op2(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op2(C, op, dst, dstw, src1, src1w, src2, src2w);
}

sljit_s32 memkit_jit_emit_op2u(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op2u(C, op, src1, src1w, src2, src2w);
}

sljit_s32 memkit_jit_emit_op2r(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op2r(C, op, dst_reg, src1, src1w, src2, src2w);
}

// ----- Instructions: Shift Into & Shifted Operand --------------------------

sljit_s32 memkit_jit_emit_shift_into(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg,
    sljit_s32 src1_reg,
    sljit_s32 src2_reg,
    sljit_s32 src3, sljit_sw src3w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_shift_into(C, op, dst_reg, src1_reg, src2_reg, src3, src3w);
}

sljit_s32 memkit_jit_emit_op2_shift(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w,
    sljit_sw shift_arg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op2_shift(C, op, dst, dstw, src1, src1w, src2, src2w, shift_arg);
}

// ----- Instructions: Source/Dest -------------------------------------------

sljit_s32 memkit_jit_emit_op_src(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op_src(C, op, src, srcw);
}

sljit_s32 memkit_jit_emit_op_dst(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op_dst(C, op, dst, dstw);
}

// ----- Floating Point Operations -------------------------------------------

sljit_s32 memkit_jit_emit_fop1(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fop1(C, op, dst, dstw, src, srcw);
}

sljit_s32 memkit_jit_emit_fop2(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fop2(C, op, dst, dstw, src1, src1w, src2, src2w);
}

struct sljit_jump *memkit_jit_emit_fcmp(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return NULL;
    return sljit_emit_fcmp(C, type, src1, src1w, src2, src2w);
}

sljit_s32 memkit_jit_emit_fselect(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_freg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2_freg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fselect(C, type, dst_freg, src1, src1w, src2_freg);
}

sljit_s32 memkit_jit_emit_fset32(struct sljit_compiler *C,
    sljit_s32 freg, sljit_f32 value) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fset32(C, freg, value);
}

sljit_s32 memkit_jit_emit_fset64(struct sljit_compiler *C,
    sljit_s32 freg, sljit_f64 value) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fset64(C, freg, value);
}

sljit_s32 memkit_jit_emit_fcopy(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 freg, sljit_s32 reg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fcopy(C, op, freg, reg);
}

sljit_s32 memkit_jit_emit_fmem(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 freg, sljit_s32 mem, sljit_sw memw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fmem(C, type, freg, mem, memw);
}

sljit_s32 memkit_jit_emit_fmem_update(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 freg, sljit_s32 mem, sljit_sw memw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fmem_update(C, type, freg, mem, memw);
}

sljit_s32 memkit_jit_emit_fop2r(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_freg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_fop2r(C, op, dst_freg, src1, src1w, src2, src2w);
}

// ----- SIMD Operations -----------------------------------------------------

sljit_s32 memkit_jit_emit_simd_mov(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 srcdst, sljit_sw srcdstw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_mov(C, type, vreg, srcdst, srcdstw);
}

sljit_s32 memkit_jit_emit_simd_replicate(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_replicate(C, type, vreg, src, srcw);
}

sljit_s32 memkit_jit_emit_simd_lane_mov(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 lane_index,
    sljit_s32 srcdst, sljit_sw srcdstw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_lane_mov(C, type, vreg, lane_index, srcdst, srcdstw);
}

sljit_s32 memkit_jit_emit_simd_lane_replicate(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_s32 src_lane_index) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_lane_replicate(C, type, vreg, src, src_lane_index);
}

sljit_s32 memkit_jit_emit_simd_extend(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_extend(C, type, vreg, src, srcw);
}

sljit_s32 memkit_jit_emit_simd_sign(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 dst, sljit_sw dstw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_sign(C, type, vreg, dst, dstw);
}

sljit_s32 memkit_jit_emit_simd_op2(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_vreg, sljit_s32 src1_vreg,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_simd_op2(C, type, dst_vreg, src1_vreg, src2, src2w);
}

// ----- Atomic Operations ---------------------------------------------------

sljit_s32 memkit_jit_emit_atomic_load(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg, sljit_s32 mem_reg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_atomic_load(C, op, dst_reg, mem_reg);
}

sljit_s32 memkit_jit_emit_atomic_store(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src_reg, sljit_s32 mem_reg, sljit_s32 temp_reg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_atomic_store(C, op, src_reg, mem_reg, temp_reg);
}

// ----- Labels & Jumps ------------------------------------------------------

struct sljit_label *memkit_jit_emit_label(struct sljit_compiler *C) {
    if (!C) return NULL;
    return sljit_emit_label(C);
}

struct sljit_jump *memkit_jit_emit_jump(struct sljit_compiler *C, sljit_s32 type) {
    if (!C) return NULL;
    return sljit_emit_jump(C, type);
}

void memkit_jit_set_label(struct sljit_jump *jump, struct sljit_label *label) {
    if (jump) {
        sljit_set_label(jump, label);
    }
}

void memkit_jit_set_target(struct sljit_jump *jump, sljit_uw target) {
    if (jump) {
        sljit_set_target(jump, target);
    }
}

struct sljit_jump *memkit_jit_emit_cmp(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return NULL;
    return sljit_emit_cmp(C, type, src1, src1w, src2, src2w);
}

sljit_s32 memkit_jit_emit_ijump(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_ijump(C, type, src, srcw);
}

struct sljit_label *memkit_jit_emit_aligned_label(struct sljit_compiler *C,
    sljit_s32 alignment, struct sljit_read_only_buffer *buffers) {
    if (!C) return NULL;
    return sljit_emit_aligned_label(C, alignment, buffers);
}

struct sljit_jump *memkit_jit_emit_op2cmpz(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w) {
    if (!C) return NULL;
    return sljit_emit_op2cmpz(C, op, dst, dstw, src1, src1w, src2, src2w);
}

// ----- Function Calls ------------------------------------------------------

struct sljit_jump *memkit_jit_emit_call(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 arg_types) {
    if (!C) return NULL;
    return sljit_emit_call(C, type, arg_types);
}

sljit_s32 memkit_jit_emit_icall(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 arg_types, sljit_s32 src, sljit_sw srcw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_icall(C, type, arg_types, src, srcw);
}

// ----- Constants & Addresses -----------------------------------------------

struct sljit_const *memkit_jit_emit_const(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw, sljit_sw init_value) {
    if (!C) return NULL;
    return sljit_emit_const(C, op, dst, dstw, init_value);
}

struct sljit_jump *memkit_jit_emit_op_addr(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw) {
    if (!C) return NULL;
    return sljit_emit_op_addr(C, op, dst, dstw);
}

void memkit_jit_set_const(sljit_uw const_addr, sljit_s32 op,
    sljit_sw new_constant, sljit_sw executable_offset) {
    sljit_set_const(const_addr, op, new_constant, executable_offset);
}

void memkit_jit_set_jump_addr(sljit_uw addr, sljit_uw new_target,
    sljit_sw executable_offset) {
    sljit_set_jump_addr(addr, new_target, executable_offset);
}

// ----- Memory & Locals -----------------------------------------------------

sljit_s32 memkit_jit_get_local_base(struct sljit_compiler *C,
    sljit_s32 dst, sljit_sw dstw, sljit_sw offset) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_get_local_base(C, dst, dstw, offset);
}

sljit_s32 memkit_jit_emit_mem(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 reg, sljit_s32 mem, sljit_sw memw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_mem(C, type, reg, mem, memw);
}

sljit_s32 memkit_jit_emit_mem_update(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 reg, sljit_s32 mem, sljit_sw memw) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_mem_update(C, type, reg, mem, memw);
}

// ----- Flags & Conditions --------------------------------------------------

sljit_s32 memkit_jit_emit_op_flags(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw, sljit_s32 type) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op_flags(C, op, dst, dstw, type);
}

sljit_s32 memkit_jit_emit_select(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_reg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2_reg) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_select(C, type, dst_reg, src1, src1w, src2_reg);
}

// ----- Custom Instructions --------------------------------------------------

sljit_s32 memkit_jit_emit_op_custom(struct sljit_compiler *C,
    void *instruction, sljit_u32 size) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    if (!instruction) return SLJIT_ERR_BAD_ARGUMENT;
    return sljit_emit_op_custom(C, instruction, size);
}

// ----- Query Functions -----------------------------------------------------

sljit_s32 memkit_jit_cmp_info(sljit_s32 type) {
    return sljit_cmp_info(type);
}

void memkit_jit_set_current_flags(struct sljit_compiler *C,
    sljit_s32 current_flags) {
    if (C) {
        sljit_set_current_flags(C, current_flags);
    }
}

// ----- Code Introspection --------------------------------------------------

sljit_sw memkit_jit_get_executable_offset(struct sljit_compiler *C) {
    if (!C) return 0;
    return sljit_get_executable_offset(C);
}

sljit_uw memkit_jit_get_generated_code_size(struct sljit_compiler *C) {
    if (!C) return 0;
    return sljit_get_generated_code_size(C);
}

sljit_uw memkit_jit_get_label_addr(struct sljit_label *label) {
    if (!label) return 0;
    return sljit_get_label_addr(label);
}

sljit_uw memkit_jit_get_jump_addr(struct sljit_jump *jump) {
    if (!jump) return 0;
    return sljit_get_jump_addr(jump);
}

sljit_uw memkit_jit_get_const_addr(struct sljit_const *const_) {
    if (!const_) return 0;
    return sljit_get_const_addr(const_);
}

sljit_uw memkit_jit_get_label_index(struct sljit_label *label) {
    if (!label) return 0;
    return sljit_get_label_index(label);
}

sljit_s32 memkit_jit_jump_has_label(struct sljit_jump *jump) {
    if (!jump) return 0;
    return sljit_jump_has_label(jump);
}

sljit_s32 memkit_jit_jump_has_target(struct sljit_jump *jump) {
    if (!jump) return 0;
    return sljit_jump_has_target(jump);
}

sljit_s32 memkit_jit_jump_is_mov_addr(struct sljit_jump *jump) {
    if (!jump) return 0;
    return sljit_jump_is_mov_addr(jump);
}

struct sljit_label *memkit_jit_get_first_label(struct sljit_compiler *C) {
    if (!C) return NULL;
    return sljit_get_first_label(C);
}

struct sljit_jump *memkit_jit_get_first_jump(struct sljit_compiler *C) {
    if (!C) return NULL;
    return sljit_get_first_jump(C);
}

struct sljit_const *memkit_jit_get_first_const(struct sljit_compiler *C) {
    if (!C) return NULL;
    return sljit_get_first_const(C);
}

struct sljit_label *memkit_jit_get_next_label(struct sljit_label *label) {
    if (!label) return NULL;
    return sljit_get_next_label(label);
}

struct sljit_jump *memkit_jit_get_next_jump(struct sljit_jump *jump) {
    if (!jump) return NULL;
    return sljit_get_next_jump(jump);
}

struct sljit_const *memkit_jit_get_next_const(struct sljit_const *const_) {
    if (!const_) return NULL;
    return sljit_get_next_const(const_);
}

// ----- Compiler Utilities --------------------------------------------------

sljit_s32 memkit_jit_set_compiler_memory_error(struct sljit_compiler *C) {
    if (!C) return SLJIT_ERR_BAD_ARGUMENT;
    sljit_set_compiler_memory_error(C);
    return SLJIT_SUCCESS;
}

void *memkit_jit_alloc_memory(struct sljit_compiler *C, sljit_s32 size) {
    if (!C) return NULL;
    return sljit_alloc_memory(C, size);
}

void memkit_jit_compiler_set_user_data(struct sljit_compiler *C, void *user_data) {
    if (C) {
        sljit_compiler_set_user_data(C, user_data);
    }
}

void *memkit_jit_compiler_get_user_data(struct sljit_compiler *C) {
    if (!C) return NULL;
    return sljit_compiler_get_user_data(C);
}

// ----- Register Info -------------------------------------------------------

sljit_s32 memkit_jit_get_register_index(sljit_s32 type, sljit_s32 reg) {
    return sljit_get_register_index(type, reg);
}

// ----- Serialization (AOT) -------------------------------------------------

sljit_uw *memkit_jit_serialize_compiler(struct sljit_compiler *C,
    sljit_s32 options, sljit_uw *size) {
    if (!C) return NULL;
    return sljit_serialize_compiler(C, options, size);
}

struct sljit_compiler *memkit_jit_deserialize_compiler(sljit_uw *buffer,
    sljit_uw size, sljit_s32 options) {
    return sljit_deserialize_compiler(buffer, size, options, NULL);
}

// ----- Stack Management ----------------------------------------------------

struct sljit_stack *memkit_jit_allocate_stack(sljit_uw start_size, sljit_uw max_size) {
    return sljit_allocate_stack(start_size, max_size, NULL);
}

void memkit_jit_free_stack(struct sljit_stack *stack) {
    if (stack) {
        sljit_free_stack(stack, NULL);
    }
}

sljit_u8 *memkit_jit_stack_resize(struct sljit_stack *stack, sljit_u8 *new_start) {
    if (!stack) return NULL;
    return sljit_stack_resize(stack, new_start);
}

// ----- Executable Memory ---------------------------------------------------

void memkit_jit_free_unused_memory_exec(void) {
#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
    sljit_free_unused_memory_exec();
#endif
}
