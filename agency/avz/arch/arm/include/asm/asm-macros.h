/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __ASM_ARM_ASM_MACROS_H
#define __ASM_ARM_ASM_MACROS_H

#include <soo/uapi/arch-arm.h>

#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/system.h>

#include <avz/config.h>

#include <generated/asm-offsets.h>

#ifdef __ASSEMBLY__
/*
 * Endian independent macros for shifting bytes within registers.
 */
#ifndef __ARMEB__
#define pull            lsr
#define push            lsl
#define get_byte_0      lsl #0
#define get_byte_1      lsr #8
#define get_byte_2      lsr #16
#define get_byte_3      lsr #24
#define put_byte_0      lsl #0
#define put_byte_1      lsl #8
#define put_byte_2      lsl #16
#define put_byte_3      lsl #24
#else
#define pull            lsl
#define push            lsr
#define get_byte_0      lsr #24
#define get_byte_1      lsr #16
#define get_byte_2      lsr #8
#define get_byte_3      lsl #0
#define put_byte_0      lsl #24
#define put_byte_1      lsl #16
#define put_byte_2      lsl #8
#define put_byte_3      lsl #0
#endif


#ifdef __ASSEMBLY__

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR


#define ENTRY(name) \
  .globl name; \
  ALIGN; \
  name:

#define PRIVATE(name)	\
	.align 5;	\
	name:
#endif

/*
 * LOADREGS - ldm with PC in register list (eg, ldmfd sp!, {pc})
 */
#ifdef __STDC__
#define LOADREGS(cond, base, reglist...)\
        ldm##cond       base,reglist
#else
#define LOADREGS(cond, base, reglist...)\
        ldm/**/cond     base,reglist
#endif

/*
 * Build a return instruction for this processor type.
 */
#define RETINSTR(instr, regs...)\
        instr   regs

@
@ Stack format (ensured by USER_* and SVC_*)
@
#define S_FRAME_SIZE    72
#define S_CONTEXT	68
#define S_PSR           64
#define S_PC            60
#define S_LR            56
#define S_SP            52
#define S_IP            48
#define S_FP            44
#define S_R10           40
#define S_R9            36
#define S_R8            32
#define S_R7            28
#define S_R6            24
#define S_R5            20
#define S_R4            16
#define S_R3            12
#define S_R2            8
#define S_R1            4
#define S_R0            0

	.macro	mask_pc, rd, rm
	.endm

/*
 * These are the registers used in the syscall handler, and allow us to
 * have in theory up to 7 arguments to a function - r0 to r6.
 *
 * r7 is reserved for the system call number for thumb mode.
 *
 * Note that tbl == why is intentional.
 *
 * We must set at least "tsk" and "why" when calling ret_with_reschedule.
 */
scno    .req    r7              @ syscall number
tbl     .req    r8              @ syscall table pointer
why     .req    r8              @ Linux syscall (!= 0)
tsk     .req    r9              @ current thread_info

	.macro	topstack	rd
	  ldr	\rd, =(~(STACK_SIZE - 1))
	  and	\rd, r13, \rd
	.endm

	.macro	vcpu	rd
		ldr	\rd, =(~(STACK_SIZE - 1))
		and	\rd, r13, \rd
		ldr	\rd, [\rd]
	.endm

	.macro	set_upcall_mask		rd
	.endm

	.macro	clear_upcall_mask	rd
	.endm

	.macro __local_save_flags  temp_int
	mrs    \temp_int,  cpsr                @ local_save_flags
	.endm

	.macro __local_irq_resotre  temp_int
	msr    cpsr_c, \temp_int              @ local_irq_restore
	.endm

	.macro __local_irq_save  flag tmp
	mrs     \flag, cpsr
	orr     \tmp, \flag, #128
	msr     cpsr, \tmp
	.endm

	.macro __local_irq_restore  flag
	msr     cpsr_c, \flag
	.endm

/*
 * These two are used to save LR/restore PC over a user-based access.
 * The old 26-bit architecture requires that we do.  On 32-bit
 * architecture, we can safely ignore this requirement.
 */
	.macro	save_lr
	.endm

	.macro	restore_pc
	mov	pc, lr
	.endm

	.macro spin_forever
	1:
	mov	pc, 1b
	.endm

	.macro current_cpu reg
	mrc p15, 0, \reg, c0, c0, 5 	@ read Multiprocessor ID register reg
	and \reg, \reg, #0x3	@ mask on CPU ID bits
	.endm
#endif
#endif

