/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <soo/uapi/arch-arm.h>

#include <asm/asm-macros.h>
#include <asm/page.h>
#include <asm/system.h>
#include <avz/config.h>

#include "entry-header.S"
#include <generated/asm-offsets.h>
#include <asm/domain.h>

	.extern hypervisor_stack
	.extern pseudo_usr_mode
	.extern get_softirq_stat

	.align	5

@ This function is called at bootstrap and
@ reboot time. It initializes some registers
ENTRY(pre_ret_to_user)
	disable_irq

	vcpu	r10
	mov 	r6, #0
	str		r6, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_HYPERVISOR_CALLBACK)]

	current_cpu r3

#warning must be investigated and probably be removed (CPU init part)
#if CONFIG_CPU_V7
	@ still some reconfig for V7
	ldr	r0, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_GUEST_TTBR0)]
    ldr	r1, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_GUEST_TTBR1)]
	ldr r2, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_GUEST_CONTEXTID)]
    @ Call arch-dependent ttb setup code in case where some special bits are necessary...
	bl	_v7_ttb_setup

	mrc	p15, 0, r2, c2, c0, 2
	str	r2, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_GUEST_TTBCR)]
#endif

	@ switch to the guest stack
	ldr r0, .LChypervisor_stack
	str	sp, [r0, r3, lsl #2]

	ldr	r6,	[sp, #S_PC]		@ entry point of the guess  /  r6 is used because not altered by save_svc_context
	ldr r7, [sp, #S_PSR]
	ldr r8, [sp, #S_IP]  	@ start_info (r12)
	ldr r9, [sp, #S_R2]		@ arg (devtree/atags)

	ldr 	sp, [sp, #S_SP]   	@ get the guest stack
	sub	sp, sp, #S_FRAME_SIZE   @ will be restored later, but the contents is not important
	str	sp, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

	@ preserve the guest stack before coming back to the hypervisor stack

	str r6, [sp, #S_PC]	@ put the entry point on the guess stack
	str r7, [sp, #S_PSR]
	str r8, [sp, #S_IP]
	str r9, [sp, #S_R2]

	@ back to hypervisor stack
	ldr r0, .LChypervisor_stack
	ldr sp, [r0, r3, lsl #2]
	ldr	r3, [sp, #S_R3]
	b restore

/*
 * The following function is used to restore the migrated domain.
 * Indeed, the receiver environment has not saved anything on its stack regarding
 * a context switch. We can not pursue on right-after-context-switch in the schedule function!
 * But we do not start from boot either. So, we have an appropriate restore glue code to perform
 * an upcall in the newly migrated ME. A first timer IRQ has been set in domain_migration_restore()to
 * avoid a problem in the guest when testing for upcall pending.
 *
 * In case of a realtime ME, a ping DC_RESUME might be issued before the guest had time to be in its kernel code.
 * That's why we need to proceed with a do_upcall (and not simply restore).
 *
 */
ENTRY(after_migrate_to_user)

    @ should be enough

    @ We need to set up a correct vector offset in S_CONTEXT

    current_cpu r11

    vcpu    r10

	ldr		r0, .LChypervisor_stack   	@ running SVC hypervisor stack
	str		sp, [r0, r11, lsl #2]

	@ get guest stack (already stacked from save_svc_context)
	ldr		sp, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

	mov		r9, #0x18   @ IRQ -> will drive to evtchn_do_upcall() in the guest
    str		r9, [sp, #S_CONTEXT]

	ldr		sp, [r0, r11, lsl #2]

	b do_upcall

ENTRY(ret_to_user)

	disable_irq 					@ ensure IRQs are disabled

	bl	do_softirq

	vcpu	r10
	ldr		r11, [r10, #OFFSET_VCPU_INFO]

	@ If the softirq handling leads to trigger an interrupt in the guest,
	@ it will be processed by do_evtchn_do_upcall. The way how to
	@ process an interrupt with potentially IRQs off is under the
	@ responsibility of the guest

	@ are some IRQs pending?
	ldrb	r12, [r11, #OFFSET_EVTCHN_UPCALL_PENDING]
	tst		r12, #0xff

	beq	restore

	b	do_upcall

/*
 * Send event to guest domain
 */
ENTRY(do_upcall)
	disable_irq

	current_cpu r11

	vcpu    r10

	ldr		lr, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_HYPERVISOR_CALLBACK)]
	cmp		lr, #0
	beq		restore

	ldr		r0, .LChypervisor_stack   	@ running SVC hypervisor stack
	str		sp, [r0, r11, lsl #2]

	@ get guest stack (already stacked from save_svc_context)
	ldr		sp, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

	@ setting pseudo_usr_mode / r0, r1 re-assigned right after
	ldr 	r0, .LCpseudo_usr_mode
	mov		r1, #1
	str		r1, [r0, r11, lsl #2]

	@ r0 contains a reference to the stack pointer
	mov		r0, sp

	ldr		r1, [sp, #S_R1]

	mov		pc, lr

ENTRY(restore)

	current_cpu r11

	@ setting pseudo_usr_mode / r0, r1 re-assigned right after
	ldr 	r0, .LCpseudo_usr_mode
	mov		r1, #1
	str		r1, [r0, r11, lsl #2]


	@ restore saved registers

	ldr		r0, .LChypervisor_stack   	@ running SVC hypervisor stack
	str		sp, [r0, r11, lsl #2]

	vcpu	r10

	@ get guest stack (already stacked from save_svc_context)
	ldr		sp, [r10, #(OFFSET_ARCH_VCPU + OFFSET_GUEST_CONTEXT + OFFSET_SYS_REGS + OFFSET_VKSP)]

	ldr		r0, [sp, #S_PSR]			@ Check if return is in guest SVC or guest USR
	msr		spsr_cxsf, r0

	and		r0, r0, #PSR_MODE_MASK
	cmp		r0, #PSR_MODE_USR   			@ usr ?
	bne		restore_svc

	ldr     lr, [sp, #S_PC]!                @ Get PC

	ldmdb   sp, {r0 - lr}^                  @ Get calling r0 - lr
	mov     r0, r0
	add     sp, sp, #S_FRAME_SIZE - S_PC

	movs    pc, lr                          @ return & move spsr_svc into cpsr

restore_svc:

	ldmia	sp, {r0 - pc}^		@ load r0 - pc, cpsr


.LCpseudo_usr_mode:
	.word	pseudo_usr_mode

.LChypervisor_stack:
	.word	hypervisor_stack


