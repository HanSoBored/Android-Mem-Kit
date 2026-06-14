#ifndef MEMKIT_JIT_H
#define MEMKIT_JIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "sljitLir.h"

// ============================================================================
// TIER 1: THIN WRAPPERS (1:1 mapping to SLJIT functions)
// ============================================================================
// These follow the exact SLJIT semantics with memkit_jit_ prefix.
// See sljitLir.h for full documentation on each function.
//
// Error handling: All emit functions return sljit_s32 (SLJIT_SUCCESS on success).
// After an error, all subsequent emit calls on the same compiler return early.
// Check errors at code generation time with memkit_jit_get_error().
// ============================================================================

// ----- Lifecycle -----------------------------------------------------------

struct sljit_compiler *memkit_jit_create_compiler(void);

void memkit_jit_destroy_compiler(struct sljit_compiler *C);

sljit_s32 memkit_jit_get_error(struct sljit_compiler *C);

void *memkit_jit_generate_code(struct sljit_compiler *C);

void memkit_jit_free_code(void *code);

sljit_s32 memkit_jit_has_cpu_feature(sljit_s32 feature_type);

const char *memkit_jit_get_platform_name(void);

// ----- Function Entry/Exit -------------------------------------------------

sljit_s32 memkit_jit_emit_enter(struct sljit_compiler *C, sljit_s32 options,
    sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds, sljit_s32 local_size);

sljit_s32 memkit_jit_set_context(struct sljit_compiler *C, sljit_s32 options,
    sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds, sljit_s32 local_size);

sljit_s32 memkit_jit_emit_return_void(struct sljit_compiler *C);

sljit_s32 memkit_jit_emit_return(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src, sljit_sw srcw);

sljit_s32 memkit_jit_emit_return_to(struct sljit_compiler *C,
    sljit_s32 src, sljit_sw srcw);

// ----- Instructions: Zero Operand ------------------------------------------

sljit_s32 memkit_jit_emit_op0(struct sljit_compiler *C, sljit_s32 op);

// ----- Instructions: One Operand -------------------------------------------

sljit_s32 memkit_jit_emit_op1(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src, sljit_sw srcw);

// ----- Instructions: Two Operands ------------------------------------------

sljit_s32 memkit_jit_emit_op2(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

sljit_s32 memkit_jit_emit_op2u(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

sljit_s32 memkit_jit_emit_op2r(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

// ----- Instructions: Shift Into & Shifted Operand --------------------------

sljit_s32 memkit_jit_emit_shift_into(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg,
    sljit_s32 src1_reg,
    sljit_s32 src2_reg,
    sljit_s32 src3, sljit_sw src3w);

sljit_s32 memkit_jit_emit_op2_shift(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w,
    sljit_sw shift_arg);

// ----- Instructions: Source/Dest -------------------------------------------

sljit_s32 memkit_jit_emit_op_src(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src, sljit_sw srcw);

sljit_s32 memkit_jit_emit_op_dst(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw);

// ----- Floating Point Operations -------------------------------------------

sljit_s32 memkit_jit_emit_fop1(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src, sljit_sw srcw);

sljit_s32 memkit_jit_emit_fop2(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

struct sljit_jump *memkit_jit_emit_fcmp(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

sljit_s32 memkit_jit_emit_fselect(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_freg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2_freg);

sljit_s32 memkit_jit_emit_fset32(struct sljit_compiler *C,
    sljit_s32 freg, sljit_f32 value);

sljit_s32 memkit_jit_emit_fset64(struct sljit_compiler *C,
    sljit_s32 freg, sljit_f64 value);

sljit_s32 memkit_jit_emit_fcopy(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 freg, sljit_s32 reg);

sljit_s32 memkit_jit_emit_fmem(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 freg, sljit_s32 mem, sljit_sw memw);

sljit_s32 memkit_jit_emit_fmem_update(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 freg, sljit_s32 mem, sljit_sw memw);

sljit_s32 memkit_jit_emit_fop2r(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_freg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

// ----- SIMD Operations -----------------------------------------------------

sljit_s32 memkit_jit_emit_simd_mov(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 srcdst, sljit_sw srcdstw);

sljit_s32 memkit_jit_emit_simd_replicate(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_sw srcw);

sljit_s32 memkit_jit_emit_simd_lane_mov(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 lane_index,
    sljit_s32 srcdst, sljit_sw srcdstw);

sljit_s32 memkit_jit_emit_simd_lane_replicate(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_s32 src_lane_index);

sljit_s32 memkit_jit_emit_simd_extend(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 src, sljit_sw srcw);

sljit_s32 memkit_jit_emit_simd_sign(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 vreg, sljit_s32 dst, sljit_sw dstw);

sljit_s32 memkit_jit_emit_simd_op2(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_vreg, sljit_s32 src1_vreg,
    sljit_s32 src2, sljit_sw src2w);

// ----- Atomic Operations ---------------------------------------------------

sljit_s32 memkit_jit_emit_atomic_load(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst_reg, sljit_s32 mem_reg);

sljit_s32 memkit_jit_emit_atomic_store(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 src_reg, sljit_s32 mem_reg, sljit_s32 temp_reg);

// ----- Labels & Jumps ------------------------------------------------------

struct sljit_label *memkit_jit_emit_label(struct sljit_compiler *C);

struct sljit_jump *memkit_jit_emit_jump(struct sljit_compiler *C, sljit_s32 type);

void memkit_jit_set_label(struct sljit_jump *jump, struct sljit_label *label);

void memkit_jit_set_target(struct sljit_jump *jump, sljit_uw target);

struct sljit_jump *memkit_jit_emit_cmp(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

sljit_s32 memkit_jit_emit_ijump(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 src, sljit_sw srcw);

struct sljit_label *memkit_jit_emit_aligned_label(struct sljit_compiler *C,
    sljit_s32 alignment, struct sljit_read_only_buffer *buffers);

struct sljit_jump *memkit_jit_emit_op2cmpz(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2, sljit_sw src2w);

// ----- Function Calls ------------------------------------------------------

struct sljit_jump *memkit_jit_emit_call(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 arg_types);

sljit_s32 memkit_jit_emit_icall(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 arg_types, sljit_s32 src, sljit_sw srcw);

// ----- Constants & Addresses -----------------------------------------------

struct sljit_const *memkit_jit_emit_const(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw, sljit_sw init_value);

struct sljit_jump *memkit_jit_emit_op_addr(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw);

void memkit_jit_set_const(sljit_uw const_addr, sljit_s32 op,
    sljit_sw new_constant, sljit_sw executable_offset);

void memkit_jit_set_jump_addr(sljit_uw addr, sljit_uw new_target,
    sljit_sw executable_offset);

// ----- Memory & Locals -----------------------------------------------------

sljit_s32 memkit_jit_get_local_base(struct sljit_compiler *C,
    sljit_s32 dst, sljit_sw dstw, sljit_sw offset);

sljit_s32 memkit_jit_emit_mem(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 reg, sljit_s32 mem, sljit_sw memw);

sljit_s32 memkit_jit_emit_mem_update(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 reg, sljit_s32 mem, sljit_sw memw);

// ----- Flags & Conditions --------------------------------------------------

sljit_s32 memkit_jit_emit_op_flags(struct sljit_compiler *C, sljit_s32 op,
    sljit_s32 dst, sljit_sw dstw, sljit_s32 type);

sljit_s32 memkit_jit_emit_select(struct sljit_compiler *C, sljit_s32 type,
    sljit_s32 dst_reg,
    sljit_s32 src1, sljit_sw src1w,
    sljit_s32 src2_reg);

// ----- Custom Instructions --------------------------------------------------

sljit_s32 memkit_jit_emit_op_custom(struct sljit_compiler *C,
    void *instruction, sljit_u32 size);

// ----- Query Functions -----------------------------------------------------

sljit_s32 memkit_jit_cmp_info(sljit_s32 type);

void memkit_jit_set_current_flags(struct sljit_compiler *C,
    sljit_s32 current_flags);

// ----- Code Introspection --------------------------------------------------

sljit_sw memkit_jit_get_executable_offset(struct sljit_compiler *C);

sljit_uw memkit_jit_get_generated_code_size(struct sljit_compiler *C);

sljit_uw memkit_jit_get_label_addr(struct sljit_label *label);

sljit_uw memkit_jit_get_jump_addr(struct sljit_jump *jump);

sljit_uw memkit_jit_get_const_addr(struct sljit_const *const_);

sljit_uw memkit_jit_get_label_index(struct sljit_label *label);

sljit_s32 memkit_jit_jump_has_label(struct sljit_jump *jump);

sljit_s32 memkit_jit_jump_has_target(struct sljit_jump *jump);

sljit_s32 memkit_jit_jump_is_mov_addr(struct sljit_jump *jump);

struct sljit_label *memkit_jit_get_first_label(struct sljit_compiler *C);

struct sljit_jump *memkit_jit_get_first_jump(struct sljit_compiler *C);

struct sljit_const *memkit_jit_get_first_const(struct sljit_compiler *C);

struct sljit_label *memkit_jit_get_next_label(struct sljit_label *label);

struct sljit_jump *memkit_jit_get_next_jump(struct sljit_jump *jump);

struct sljit_const *memkit_jit_get_next_const(struct sljit_const *const_);

// ----- Compiler Utilities --------------------------------------------------

sljit_s32 memkit_jit_set_compiler_memory_error(struct sljit_compiler *C);

void *memkit_jit_alloc_memory(struct sljit_compiler *C, sljit_s32 size);

void memkit_jit_compiler_set_user_data(struct sljit_compiler *C, void *user_data);

void *memkit_jit_compiler_get_user_data(struct sljit_compiler *C);

// ----- Register Info -------------------------------------------------------

sljit_s32 memkit_jit_get_register_index(sljit_s32 type, sljit_s32 reg);

// ----- Serialization (AOT) -------------------------------------------------

sljit_uw *memkit_jit_serialize_compiler(struct sljit_compiler *C,
    sljit_s32 options, sljit_uw *size);

struct sljit_compiler *memkit_jit_deserialize_compiler(sljit_uw *buffer,
    sljit_uw size, sljit_s32 options);

// ----- Stack Management ----------------------------------------------------

struct sljit_stack *memkit_jit_allocate_stack(sljit_uw start_size, sljit_uw max_size);

void memkit_jit_free_stack(struct sljit_stack *stack);

sljit_u8 *memkit_jit_stack_resize(struct sljit_stack *stack, sljit_u8 *new_start);

// ----- Executable Memory ---------------------------------------------------

void memkit_jit_free_unused_memory_exec(void);

// ============================================================================
// TIER 2: HIGH-LEVEL WRAPPERS
// ============================================================================
// These simplify common use-cases in memory manipulation and security research.
// They build on top of Tier 1 wrappers and raw SLJIT.
// ============================================================================

void *memkit_jit_forwarder_create(void *target, int num_args);

void *memkit_jit_forwarder_create_explicit(void *target,
    sljit_s32 arg_types, sljit_s32 num_scratches);

sljit_s32 memkit_jit_emit_nops(struct sljit_compiler *C, sljit_s32 nop_count);

void *memkit_jit_alloc_exec(size_t size);

void memkit_jit_free_exec(void *ptr, size_t size);

bool memkit_jit_write_exec(void *ptr, const void *data, size_t size);

#endif /* MEMKIT_JIT_H */
