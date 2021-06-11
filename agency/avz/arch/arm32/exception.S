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

#include <config.h>

#include <asm/mmu.h>
#include <asm/processor.h>

#include <generated/asm-offsets.h>

.globl  pseudo_usr_mode
.globl	hypervisor_stack

.extern __prefetch_abort
.extern __data_abort
.extern __undefined_instruction

#if (S_FRAME_SIZE & 7)
#error "S_FRAME_SIZE must be a multiple of 8"
#endif

/* use the special section (.vectors.text), to enable fine-tuning
 * of the placement of this section inside the linker script
 */
.section ".vectors.text", "ax"

ENTRY(__vectors)
		    b   _start @ reset
            ldr pc, _undefined_instruction
            ldr pc, _syscall_interrupt
            ldr pc, _prefetch_abort
            ldr pc, _data_abort
            ldr pc, _not_used
            ldr pc, _irq
            ldr pc, _fiq

_undefined_instruction: .word undefined_instruction
_syscall_interrupt:     .word not_used
_prefetch_abort:        .word prefetch_abort
_data_abort:            .word data_abort
_not_used:              .word not_used
_irq:                   .word irq
_fiq:                   .word not_used
_pad:                   .word 0x12345678 @ now 16*4=64

.align	5

@ This function is called at bootstrap and
@ reboot time. It initializes some registers
ENTRY(pre_ret_to_user)
	disable_irq

	curdom	r10
	mov 	r6, #0
	str		r6, [r10, #OFFSET_HYPERVISOR_CALLBACK]

	current_cpu r3

	@ Prepare to switch to the guest stack in order to prepare
	@ its stack frame required along the upcall (restore) path.

	ldr 	r0, .LChypervisor_stack
	str		sp, [r0, r3, lsl #2]

	ldr		r6,	[sp, #OFFSET_PC]	@ entry point of the guess  /  r6 is used because not altered by save_svc_context
	ldr 	r7, [sp, #OFFSET_PSR]	@ CPSR flags
	ldr 	r8, [sp, #OFFSET_IP]  	@ start_info (r12)
	ldr 	r9, [sp, #OFFSET_R2]	@ arg (devtree/atags)

	ldr 	sp, [sp, #OFFSET_SP]   	@ get the guest stack
	sub		sp, sp, #S_FRAME_SIZE

	str		sp, [r10, #OFFSET_G_SP]

	@ Configure the G-stack frame

	str 	r6, [sp, #OFFSET_PC]	@ put the entry point on the guess stack
	str 	r7, [sp, #OFFSET_PSR]
	str 	r8, [sp, #OFFSET_IP]
	str 	r9, [sp, #OFFSET_R2]

	@ Finally switch back to H-stack
	ldr 	r0, .LChypervisor_stack
	ldr 	sp, [r0, r3, lsl #2]

	@ Restore r3 used by current_cpu above
	ldr		r3, [sp, #OFFSET_R3]

	b restore

/*
 * The following function is used to restore the migrated domain.
 * Indeed, the receiver environment has not saved anything on its stack regarding
 * a context switch. We can not pursue on right-after-context-switch in the schedule function!
 * But we do not start from boot either. So, we have an appropriate restore glue code to perform
 * an upcall in the newly migrated ME. A first timer IRQ has been set in domain_migration_restore() to
 * avoid a problem in the guest when testing for upcall pending.
 *
 *
 */
ENTRY(after_migrate_to_user)

	b 		do_upcall

.align	5

/*
 * r0 = previous domain, r1 = next domain
 * previous and next are guaranteed not to be the same.
 *
 */
ENTRY(__switch_to)
	disable_irq 				@ ensure IRQs are disabled

	add     ip, r0, #(OFFSET_CPU_REGS + OFFSET_R4)
save_ctx:
    stmia   ip, {r4 - sl, fp, ip, sp, lr}      @ Store most regs on stack

	current_cpu r6

load_ctx:

	add		ip, r1, #(OFFSET_CPU_REGS + OFFSET_R4)

    ldmia   ip,  {r4 - sl, fp, ip, sp, pc}       @ Load all regs saved previously

	nop
	nop
	nop

/******** Exception handlers **********/
.align  5
undefined_instruction:

    b  __undefined_instruction

.align  5
prefetch_abort:

	@ Call the C data abort handler with the following args:
	@ r0 = IFAR, r1 = IFSR, r2 = LR

	mrc	p15, 0, r0, c6, c0, 2		@ get IFAR
	mrc	p15, 0, r1, c5, c0, 1		@ get IFSR
	mov r2, lr

    b __prefetch_abort

.align  5
data_abort:

	@ Call the C data abort handler with the following args:
	@ r0 = FAR, r1 = FSR, r2 = LR

	mrc	p15, 0, r1, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
	mov r2, lr

    b __data_abort

.align  5
not_used:

    /* Fault address information retrieval */
	mrc	p15, 0, r1, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR

	mov r2, lr
	b	__fault_trap

/* *********************** */

.align 5

irq:

    @ IRQ mode (lr_irq, sp_irq, cpsr_irq)

    @ Store original r0, and lr_irq in the IRQ stack sp = sp_irq
    @ sp_irq is a simple array declared in arch/arm/setup.c (so address can be ascending)

    str 	r0, [sp]		@ original r0
    str 	lr, [sp, #4]	@ lr_irq

    mrs 	r0, spsr 		@ to preserve irq bit
    str		r0, [sp, #8]	@ spsr_irq

    mov		r0, sp			@ to maintain a reference on sp_irq; r0 will not be affected after mode switch

    @ Now switch back to SVC. IRQs are disabled
    mrs		lr, cpsr

    bic		lr, lr, #PSR_MODE_MASK
    orr		lr, lr, #PSR_SVC_MODE

    @ switch to SVC
    msr		cpsr, lr

    @ --- SVC mode from now on ---

	@ At the exception entry, the stack must be 8-byte aligned.
	@ If it is not the case (gcc might not respect the AAPCS convention for optimization purposes),
	@ sp will be adjusted. The original sp is preserved and will be correctly restored at the exit.

	tst		sp, #0x7	@ 8-bytes aligned
	strne	sp, [sp, #(OFFSET_SP-S_FRAME_SIZE - 4)]	@ save sp
	subne	sp, sp, #4
	streq	sp, [sp, #(OFFSET_SP-S_FRAME_SIZE)]	@ save sp

	@ Alignment guard
	tst		sp, #0x7		@ 8-bytes aligned
	bne		__stack_alignment_fault

	sub		sp, sp, #S_FRAME_SIZE

	@ Store the lr_svc (before the irq)
	str		lr, [sp, #OFFSET_LR]	@ current lr_svc

    @ preserve spsr for future IRQ enabling
    ldr		lr, [r0, #8]   			@ retrieve spsr_irq
    str		lr, [sp, #OFFSET_PSR]	@ location of spsr_svc (hence cpsr right before the interrupt)

	@ Check if it is necessary to preserve sp_usr and lr_usr
	and		lr, lr, #PSR_MODE_MASK
	cmp		lr, #PSR_USR_MODE

	@ Saving user mode registers (sp_usr, lr_usr)
	addeq	lr, sp, #OFFSET_SP_USR
	stmeqia	lr, {sp, lr}^

	@ Retrieve the lr_irq to set the pc out of this routine
	ldr 	lr, [r0, #4]			@ retrieve lr_irq to set lr_svc
	sub 	lr, lr, #4				@ Adjust the lr since it is automatically set from pc (in advance of 2 instructions due to the pipeline)
	str 	lr, [sp, #OFFSET_PC]  	@ future pc at exit

	ldr 	r0, [r0]			    @ original r0

    stmia 	sp, {r0-r12}

	@ Now process the IRQ differentiating execution according to the source (dom/hyp)
	@ for managing the stack correctly

	current_cpu	r10

	ldr		r0, .LCpseudo_usr_mode
	ldr		r1, [r0, r10, lsl #2]
	cmp		r1, #0  				@ svc ?

	beq		hyp_path

	mov		r1, #0 					@ setting svc
	str		r1, [r0, r10, lsl #2]

	mov		r1, sp
	ldr 	r0, .LChypervisor_stack @ Get the running hypervisor SVC stack
	ldr		r0, [r0, r10, lsl #2]

	mov		sp, r0

	curdom	r0
	str		r1, [r0, #OFFSET_G_SP]

	@ Make sure r0 refers to the base of the stack frame
	mov		r0, sp

    bl 		irq_handle

	b		ret_to_user

hyp_path:

	mov		r0, sp

    bl 		irq_handle

    b 		out_irq


ENTRY(ret_to_user)

	disable_irq 					@ ensure IRQs are disabled

	curdom	r10
	ldr		r11, [r10, #OFFSET_SHARED_INFO]

	@ If the softirq handling leads to trigger an interrupt in the guest,
	@ it will be processed by do_evtchn_do_upcall. The way how to
	@ process an interrupt with potentially IRQs off is under the
	@ responsibility of the guest

	@ are some IRQs pending?
	ldrb	r12, [r11, #OFFSET_EVTCHN_UPCALL_PENDING]
	tst		r12, #0xff

	beq		restore

	/* Fallback to do_upcall */

/*
 * Process pending events along the upcall path to the domain.
 */
ENTRY(do_upcall)
	disable_irq

	current_cpu r11

	curdom  r10

	ldr		lr, [r10, #OFFSET_HYPERVISOR_CALLBACK]
	cmp		lr, #0
	beq		restore

	ldr		r0, .LChypervisor_stack   	@ running SVC hypervisor stack
	str		sp, [r0, r11, lsl #2]

	@ get guest stack (already stacked from save_svc_context)
	ldr		sp, [r10, #OFFSET_G_SP]

	@ setting pseudo_usr_mode / r0, r1 re-assigned right after
	ldr 	r0, .LCpseudo_usr_mode
	mov		r1, #1
	str		r1, [r0, r11, lsl #2]

	@ r0 contains a reference to the stack pointer
	mov		r0, sp

	mov		pc, lr

restore:

	current_cpu r11

	@ setting pseudo_usr_mode / r0, r1 re-assigned right after
	ldr 	r0, .LCpseudo_usr_mode
	mov		r1, #1
	str		r1, [r0, r11, lsl #2]

	@ restore saved registers

	ldr		r0, .LChypervisor_stack   	@ running SVC hypervisor stack
	str		sp, [r0, r11, lsl #2]

	curdom	r10

	@ get guest stack (already stacked from save_svc_context)
	ldr		sp, [r10, #OFFSET_G_SP]

out_irq:

    ldr 	lr, [sp, #OFFSET_PSR]	@ get the saved spsr and adjust the stack pointer
    msr		spsr, lr

	@ Check if it is necessary to restore sp_usr and lr_usr
	and		lr, lr, #PSR_MODE_MASK
	cmp		lr, #PSR_USR_MODE

	@ Restoring user mode registers (sp_usr, lr_usr)
	addeq	lr, sp, #OFFSET_SP_USR
	ldmeqia	lr, {sp, lr}^

	@ Restore registers
   	ldmia 	sp, {r0-r12}

	add		sp, sp, #OFFSET_SP

	dsb
	isb

    @ Now, we retrieve the final registers, sp will be adjusted automatically
    ldmia 	sp, {sp, lr, pc}^

	.ltorg


/** Area of memory reserved to manage domain stack and hypervisor stack (G-stack/H-stack) **/

.align 5
pseudo_usr_mode:
	.space NR_CPUS * 4

@ Hypervisor stack is used for the *current* (running) domain svc stack address
hypervisor_stack:
	.space NR_CPUS * 4


.LCpseudo_usr_mode:
	.word	pseudo_usr_mode

.LChypervisor_stack:
	.word	hypervisor_stack

