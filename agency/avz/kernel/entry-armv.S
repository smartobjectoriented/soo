/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <asm/memory.h>
#include <asm/vfpmacros.h>
#include <asm/asm-macros.h>

#include <avz/init.h>

#include "entry-header.S"
#include <generated/asm-offsets.h>
#include <asm/system.h>
#include <asm/domain.h>
#include <asm/unified.h>

.globl	cr_alignment
.globl	cr_no_alignment
.globl  pseudo_usr_mode
.globl	hypervisor_stack

.extern ll_handle_irq
.extern __fault_trap

.align 5

	.macro	svc_entry, stack_hole=0
	sub	sp, sp, #(S_FRAME_SIZE + \stack_hole)
 	tst	sp, #4
 	bicne	sp, sp, #4
	stmib	sp, {r1 - r12}

	ldmia	r0, {r1 - r3}
	add	r5, sp, #S_SP		@ here for interlock avoidance
	mov	r4, #-1			@  ""  ""      ""       ""
	add	r0, sp, #(S_FRAME_SIZE + \stack_hole)
 	addne	r0, r0, #4
	str	r1, [sp]		@ save the "real" r0 copied from the exception stack

	mov	r1, lr

	@
	@ We are now ready to fill in the remaining blanks on the stack:
	@
	@  r0 - sp_svc
	@  r1 - lr_svc
	@  r2 - lr_<exception>, already fixed up for correct return/restart
	@  r3 - spsr_<exception>
	@  r4 - orig_r0 (see pt_regs definition in ptrace.h)
	@
	stmia	r5, {r0 - r4}
	.endm

	.macro	usr_entry
	sub	sp, sp, #S_FRAME_SIZE
 	stmib	sp, {r1 - r12}

	ldmia	r0, {r1 - r3}
	add	r0, sp, #S_PC		@ here for interlock avoidance
	mov	r4, #-1			@  ""  ""     ""        ""

	str	r1, [sp]		@ save the "real" r0 copied
					@ from the exception stack

	@
	@ We are now ready to fill in the remaining blanks on the stack:
	@
	@  r2 - lr_<exception>, already fixed up for correct return/restart
	@  r3 - spsr_<exception>
	@  r4 - orig_r0 (see pt_regs definition in ptrace.h)
	@
	@ Also, separately save sp_usr and lr_usr
	@
	stmia	r0, {r2 - r4}
 	stmdb	r0, {sp, lr}^

	.endm

	.align	5
.LCcralign:  @ Not handled properly...
	.word	cr_alignment

.align 5

__invalid:
1:
	b	__vector_fault

/*
 * Interrupt handling.  Preserves r7, r8, r9
 */
.macro	irq_handler

	ldr	r1, =ll_handle_irq
	mov	r0, sp
	adr	lr, BSYM(9f)
	mov pc, r1

9:
.endm

.align 5
__irq_svc:
	svc_entry

	mov		r0, #0x18				@ IRQ
	str		r0, [sp, #S_CONTEXT]

	current_cpu	r10

	ldr		r0, .LCpseudo_usr_mode
	ldr		r1, [r0, r10, lsl #2]
	cmp		r1, #0  				@ svc ?

	beq		2f

	mov		r1, #0 					@ setting svc
	str		r1, [r0, r10, lsl #2]

	mov		r1, sp
	ldr 	        r0, .LChypervisor_stack @ Get the running hypervisor SVC stack
	ldr		r0, [r0, r10, lsl #2]

	mov		sp, r0

	vcpu	r0
	str		r1, [r0, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

	irq_handler

	b	ret_to_user

2:
	irq_handler

	@ Perform a call to do_softirq to check if it is time to raise some SOFTIRQ
	bl		do_softirq
	
	@ Update spsr in order to be ready for cpsr adjustment
	ldr	r0, [sp, #S_PSR]
	msr	spsr_cxsf, r0

	ldmia	sp, {r0 - pc}^		@ load r0 - pc, cpsr

	.ltorg
/*
 * User mode handlers
 *
 * EABI note: sp_svc is always 64-bit aligned here, so should S_FRAME_SIZE
 */

#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5) && (S_FRAME_SIZE & 7)
#error "sizeof(struct pt_regs) must be a multiple of 8"
#endif

__irq_usr:
	usr_entry

	mov		r1, #0x18				@ IRQ
	str		r1, [sp, #S_CONTEXT]

	current_cpu	r10

	ldr 	r0, .LCpseudo_usr_mode
	mov		r1, #0 					@ setting svc
	str		r1, [r0, r10, lsl #2]


	mov		r1, sp
	ldr 	r0, .LChypervisor_stack   @ Get the running hypervisor SVC stack
	ldr		sp, [r0, r10, lsl #2]

	vcpu	r0
	str		r1, [r0, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

    irq_handler

	b	ret_to_user


.global prep_switch_domain
prep_switch_domain:

        @ Call, if any, the prepare_switch_xen_domain() in the guest
  	    @ Stay 8-byte aligned
        stmfd   sp!, {r0-r6, lr}
        vcpu    r0

        ldr             r1, [r0, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_PREP_SWITCH_DOMAIN_CALLBACK)]
        cmp             r1, #0

        @ r1 must be preserved

        beq             __out_prep_switch

        current_cpu r3

        ldr             r2, .LChypervisor_stack         @ running SVC hypervisor stack
        str             sp, [r2, r3, lsl #2]

        @ get guest stack (already stacked from save_svc_context)
        ldr             sp, [r0, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

        @ setting pseudo_usr_mode / r0, r1 re-assigned right after
        ldr     r0, .LCpseudo_usr_mode
        mov             r2, #1
        str             r2, [r0, r3, lsl #2]

        adr             lr, __continue_prep_switch
        mov             pc, r1

__continue_prep_switch:

        current_cpu     r3

        ldr             r0, .LCpseudo_usr_mode
        mov             r1, #0                                  @ setting svc
        str             r1, [r0, r3, lsl #2]

        mov             r1, sp
        ldr             r2, .LChypervisor_stack @ Get the running hypervisor SVC stack
        ldr             r2, [r2, r3, lsl #2]

        mov             sp, r2

        vcpu    r0
        str             r1, [r0, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

__out_prep_switch:
        ldmfd   sp!, {r0-r6, pc}




	.align	5


/*
 * Register switch for ARMv3 and ARMv4 processors
 * r0 = previous vcpu, r1 = previous vcpu_guest_context, r2 = next vcpu_guest_context
 * previous and next are guaranteed not to be the same.
 *
 * OFFSET_SYS_REGS = offsetof(struct vcpu_guest_context, sys_regs)
 * OFFSET_USER_REGS = offsetof(struct vcpu_guest_context, usr_regs)
 */
ENTRY(__switch_to)
	disable_irq 				@ ensure IRQs are disabled

	str		sp, [r1, #(OFFSET_SYS_REGS + OFFSET_VUSP)]  @ save hyp. stack (SVC)
	add     ip, r1, #(OFFSET_USER_REGS + OFFSET_R4)
save_ctx:
    stmia   ip, {r4 - sl, fp, ip, sp, lr}      @ Store most regs on stack

	current_cpu r6

        mrc             p15, 0, r4, c13, c0, 4          @ get per_cpu register
        str             r4, [r1, #(OFFSET_SYS_REGS + OFFSET_GUEST_PER_CPU)]
	mrc		p15, 0, r4, c13, c0, 3		@ get TLS register
	str		r4, [r1, #(OFFSET_SYS_REGS + OFFSET_GUEST_TLS)]
	mrc		p15, 0, r4, c13, c0, 2		@ get user r/w TLS register
	str		r4, [r1, #(OFFSET_SYS_REGS + OFFSET_GUEST_TLS_RW)]

	mrc		p15, 0, r4, c3, c0, 0
	str		r4, [r1, #(OFFSET_SYS_REGS + OFFSET_VDACR)]

load_ctx:
	ldr		r4, .LChypervisor_stack
	ldr		r5, [r2, #(OFFSET_SYS_REGS + OFFSET_VUSP)]	@ Retrieve hyp stack
	str		r5, [r4, r6, lsl #2]

        ldr             r4, [r2, #(OFFSET_SYS_REGS + OFFSET_GUEST_PER_CPU)]
        mcr             p15, 0, r4, c13, c0, 4
	ldr		r4, [r2, #(OFFSET_SYS_REGS + OFFSET_GUEST_TLS)]
	mcr		p15, 0, r4, c13, c0, 3
	ldr		r4, [r2, #(OFFSET_SYS_REGS + OFFSET_GUEST_TLS_RW)]
	mcr		p15, 0, r4, c13, c0, 2		@ reset user r/w TLS register

	ldr		r4, [r2, #(OFFSET_SYS_REGS + OFFSET_VDACR)]
	mcr		p15, 0, r4, c3, c0, 0

	add		ip, r2, #(OFFSET_USER_REGS + OFFSET_R4)

    ldmia   ip,  {r4 - sl, fp, ip, sp, pc}       @ Load all regs saved previously

	nop
	nop
	nop

.LCpseudo_usr_mode:
	.word	pseudo_usr_mode

.LChypervisor_stack:
	.word	hypervisor_stack


/*
 * SVC mode handlers
 */
.align  5
__vector_fault:
	svc_entry
1:
	/* Fault address information retrieval */
	mrc	p15, 0, r1, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR

	mov r2, lr
	b	__fault_trap

.align  5

__INIT


/*
 * Vector stubs.
 *
 * This code is copied to 0xffff0200 so we can use branches in the
 * vectors, rather than ldr's.  Note that this code must not
 * exceed 0x300 bytes.
 *
 * Common stub entry macro:
 *   Enter in IRQ mode, spsr = SVC/USR CPSR, lr = SVC/USR PC
 *
 * SP points to a minimal amount of processor-private memory, the address
 * of which is copied into r0 for the mode specific abort handler.
 */
	.macro	vector_stub, name, mode, correction=0
	.align	5

vector_\name:
	.if \correction
	sub	lr, lr, #\correction
	.endif

	@
	@ Save r0, lr_<exception> (parent PC) and spsr_<exception>
	@ (parent CPSR)
	@
	stmia	sp, {r0, lr}		@ save r0, lr
	mrs	lr, spsr
	str	lr, [sp, #8]		@ save spsr

	@
	@ Prepare for SVC32 mode.  IRQs remain disabled.
	@
	mrs	r0, cpsr
	eor	r0, r0, #(\mode ^ SVC_MODE)
	msr	spsr_cxsf, r0

	@
	@ the branch table must immediately follow this code
	@
	and	lr, lr, #0x0f
	mov	r0, sp
	ldr	lr, [pc, lr, lsl #2]
	movs	pc, lr			@ branch to handler in SVC mode
	.endm

	.globl	__stubs_start
__stubs_start:
/*
 * Interrupt dispatcher
 */
	vector_stub	irq, IRQ_MODE, 4

	.long	__irq_usr			@  0  (USR_26 / USR_32)
	.long	__invalid			@  1  (FIQ_26 / FIQ_32)
	.long	__invalid			@  2  (IRQ_26 / IRQ_32)
	.long	__irq_svc			@  3  (SVC_26 / SVC_32)
	.long	__invalid			@  4
	.long	__invalid			@  5
	.long	__invalid			@  6
	.long	__invalid			@  7
	.long	__invalid			@  8
	.long	__invalid			@  9
	.long	__invalid			@  a
	.long	__invalid			@  b
	.long	__invalid			@  c
	.long	__invalid			@  d
	.long	__invalid			@  e
	.long	__invalid			@  f

/* Data abort dispatcher
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
	vector_stub	dabt, ABT_MODE, 8

	.long	__invalid			@  0  (USR_26 / USR_32)
	.long	__invalid			@  1  (FIQ_26 / FIQ_32)
	.long	__invalid			@  2  (IRQ_26 / IRQ_32)
	.long	__vector_fault		@  3  (SVC_26 / SVC_32)
	.long	__invalid			@  4
	.long	__invalid			@  5
	.long	__invalid			@  6
	.long	__invalid			@  7
	.long	__invalid			@  8
	.long	__invalid			@  9
	.long	__invalid			@  a
	.long	__invalid			@  b
	.long	__invalid			@  c
	.long	__invalid			@  d
	.long	__invalid			@  e
	.long	__invalid			@  f

/*
 * Prefetch abort dispatcher
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
	vector_stub	pabt, ABT_MODE, 4

	.long	__invalid			@  0 (USR_26 / USR_32)
	.long	__invalid			@  1 (FIQ_26 / FIQ_32)
	.long	__invalid			@  2 (IRQ_26 / IRQ_32)
	.long	__vector_fault		@  3 (SVC_26 / SVC_32)
	.long	__invalid			@  4
	.long	__invalid			@  5
	.long	__invalid			@  6
	.long	__invalid			@  7
	.long	__invalid			@  8
	.long	__invalid			@  9
	.long	__invalid			@  a
	.long	__invalid			@  b
	.long	__invalid			@  c
	.long	__invalid			@  d
	.long	__invalid			@  e
	.long	__invalid			@  f

/*
 * Undef instr entry dispatcher
 * Enter in UND mode, spsr = SVC/USR CPSR, lr = SVC/USR PC
 */
	vector_stub	und, UND_MODE

	.long	__invalid			@  0 (USR_26 / USR_32)
	.long	__invalid			@  1 (FIQ_26 / FIQ_32)
	.long	__invalid			@  2 (IRQ_26 / IRQ_32)
	.long	__vector_fault		@  3 (SVC_26 / SVC_32)
	.long	__invalid			@  4
	.long	__invalid			@  5
	.long	__invalid			@  6
	.long	__invalid			@  7
	.long	__invalid			@  8
	.long	__invalid			@  9
	.long	__invalid			@  a
	.long	__invalid			@  b
	.long	__invalid			@  c
	.long	__invalid			@  d
	.long	__invalid			@  e
	.long	__invalid			@  f

/*
 * We group all the following data together to optimise
 * for CPUs with separate I & D caches.
 */
	.align	5

	.globl	__stubs_end
__stubs_end:

	.equ	stubs_offset, __vectors_start + 0x200 - __stubs_start

	.globl	__vectors_start
__vectors_start:
	b	__invalid
	b	vector_und + stubs_offset
	b	__invalid
	b	vector_pabt + stubs_offset
	b	vector_dabt + stubs_offset
	b	__invalid
	b	vector_irq + stubs_offset
	b	__invalid

	.globl	__vectors_end
__vectors_end:

	.data


cr_alignment:
	.space	4
cr_no_alignment:
	.space	4

pseudo_usr_mode:
	.space NR_CPUS * 4

@Hypervisor stack is used for the *current* (running) vcpu svc stack address
hypervisor_stack:
	.space NR_CPUS * 4


