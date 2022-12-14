/*
 * Copyright (C) 2005 Stelian Pop.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/linkage.h>
#include <linux/version.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/tls.h>
#ifdef CONFIG_VFP
#include <asm/vfpmacros.h>
#endif

	.macro fpu_switch tmp
#ifdef CONFIG_VFP
#if __LINUX_ARM_ARCH__ <= 6
#ifdef CONFIG_JUMP_LABEL
9998:	nop
	.pushsection __jump_table, "aw"
	.word	9998b, 9999f, __xeno_vfp_key
	.popsection
#else
	ldr	\tmp, =elf_hwcap
	ldr	\tmp, [\tmp]
	tst	\tmp, #HWCAP_VFP
	beq	9999f
#endif
#endif
	@ Always disable VFP so we can lazily save/restore the old
	@ state. This occurs in the context of the previous thread.
	VFPFMRX \tmp, FPEXC
	bic     \tmp, \tmp, #FPEXC_EN
	VFPFMXR FPEXC, \tmp
#if __LINUX_ARM_ARCH__ <= 6
9999:
#endif
#endif
	.endm

	.text

#if defined(CONFIG_VFP) && defined(CONFIG_XENO_ARCH_FPU)
/* Copied from vfp_save_state in arch/arm/vfp/vfphw.S
 * r0 = pointer to union vfp_state, r1 = fpexc
 */
ENTRY(__asm_vfp_save)
	VFPFSTMIA	r0, r2		@ save the working registers
	VFPFMRX		r2, FPSCR	@ current status
	tst		r1, #FPEXC_EX	@ is there additional state to save?
	beq		1f
	VFPFMRX		r3, FPINST	@ FPINST (only if FPEXC.EX is set)
	tst		r1, #FPEXC_FP2V	@ is there an FPINST2 to read?
	beq		1f
	VFPFMRX		r12, FPINST2	@ FPINST2 if needed (and present)
1:
	stmia		r0, {r1, r2, r3, r12}	@ save FPEXC, FPSCR, FPINST, FPINST2
	mov		pc, lr
ENDPROC(__asm_vfp_save)

/* Copied from no_old_VFP_process in arch/arm/vfp/vfphw.S
 * r0 = pointer to union vfp_state
 * r1 = current cpu
 */
ENTRY(__asm_vfp_load)
#ifdef CONFIG_SMP
	str		r1, [r0, #VFP_CPU]
#endif
	VFPFLDMIA	r0, r2		@ reload the working registers while
					@ FPEXC is in a safe state
	ldmia		r0, {r1, r2, r3, r12}	@ load FPEXC, FPSCR, FPINST, FPINST2
	tst		r1, #FPEXC_EX	@ is there additional state to restore?
	beq		1f
	VFPFMXR		FPINST, r3	@ restore FPINST (only if FPEXC.EX is set)
	tst		r1, #FPEXC_FP2V	@ is there an FPINST2 to write?
	beq		1f
	VFPFMXR		FPINST2, r12	@ FPINST2 if needed (and present)
1:
	VFPFMXR		FPSCR, r2	@ restore status
	mov		pc, lr
ENDPROC(__asm_vfp_load)
#endif


/*
/*
 * Switch context routine.
 *
 * Registers according to the ARM procedure call standard:
 *   Reg    Description
 *   r0-r3  argument/scratch registers
 *   r4-r9  variable register
 *   r10=sl stack limit/variable register
 *   r11=fp frame pointer/variable register
 *   r12=ip intra-procedure-call scratch register
 *   r13=sp stack pointer (auto preserved)
 *   r14=lr link register
 *   r15=pc program counter (auto preserved)
 *
 * Copied from __switch_to, arch/arm/kernel/entry-armv.S.
 * Right now it is identical, but who knows what the
 * future reserves us...
 *
 * XXX: All the following config options are NOT tested:
 *      CONFIG_IWMMXT
 *
 *  Calling args:
 * r0 = previous thread_info, r1 = next thread_info
 */
ENTRY(__asm_thread_switch)
	mov     ip, r0
 	stmia	ip!, {r4 - sl, fp, sp, lr} 	@ Store most regs on stack

	fpu_switch r4

    mov		r4, r1
 	ldmia	r4, {r4 - sl, fp, sp, pc}  	@ Load all regs saved previously

ENDPROC(__asm_thread_switch)
