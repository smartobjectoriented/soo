/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <generated/asm-offsets.h>

#include <asm/processor.h>

.extern do_upcall

.global __vectors

.globl  pseudo_usr_mode
.globl	hypervisor_stack
.global upcall_path

/*
 * Four types of exceptions:
 * - synchronous: aborts from MMU, SP/CP alignment checking, unallocated
 *   instructions, SVCs/SMCs/HVCs, ...)
 * - IRQ: group 1 (normal) interrupts
 * - FIQ: group 0 or secure interrupts
 * - SError: fatal system errors
 *
 * Four different contexts:
 * - from same exception level, when using the SP_EL0 stack pointer
 * - from same exception level, when using the SP_ELx stack pointer
 * - from lower exception level, when this is AArch64
 * - from lower exception level, when this is AArch32
 *
 * +------------------+------------------+-------------------------+
 * |     Address      |  Exception type  |       Description       |
 * +------------------+------------------+-------------------------+
 * | VBAR_ELn + 0x000 | Synchronous      | Current EL with SP0     |
 * |          + 0x080 | IRQ / vIRQ       |                         |
 * |          + 0x100 | FIQ / vFIQ       |                         |
 * |          + 0x180 | SError / vSError |                         |
 * +------------------+------------------+-------------------------+
 * |          + 0x200 | Synchronous      | Current EL with SPx     |
 * |          + 0x280 | IRQ / vIRQ       |                         |
 * |          + 0x300 | FIQ / vFIQ       |                         |
 * |          + 0x380 | SError / vSError |                         |
 * +------------------+------------------+-------------------------+
 * |          + 0x400 | Synchronous      | Lower EL using AArch64  |
 * |          + 0x480 | IRQ / vIRQ       |                         |
 * |          + 0x500 | FIQ / vFIQ       |                         |
 * |          + 0x580 | SError / vSError |                         |
 * +------------------+------------------+-------------------------+
 * |          + 0x600 | Synchronous      | Lower EL using AArch32  |
 * |          + 0x680 | IRQ / vIRQ       |                         |
 * |          + 0x700 | FIQ / vFIQ       |                         |
 * |          + 0x780 | SError / vSError |                         |
 * +------------------+------------------+-------------------------+
 */

/* use the special section (.vectors.text), to enable fine-tuning
 * of the placement of this section inside the linker script
 */
.section ".vectors.text", "ax"

	b __start  // To be compliant with reset vector (unavailable in aarch64)

.align 11
ENTRY(__vectors)

	// Current EL with SP0 / Synchronous
	.align 7

	mov	x0, lr
	b trap_handle

	// Current EL with SP0 / IRQ
	.align 7

	mov	x0, lr
	b trap_handle

	// Current EL with SP0 / FIQ
	.align 7

	mov	x0, lr
	b trap_handle

	// Current EL with SP0 / SError
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Current EL with SPx / Synchronous
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Current EL with SPx / IRQ
	.align 7

	b 	el01_irq_handler

	// Current EL with SPx / FIQ
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Current EL with SPx / SError
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch64 / Synchronous
	.align 7

	b	el0_sync_handler

	// Lower EL using AArch64 / IRQ
	.align 7

	b 	el01_irq_handler

	// Lower EL using AArch64 / FIQ
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch64 / SError
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch32 / Synchronous
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch32 / IRQ
	.align 7

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch32 / FIQ
	.align 7

	mov x0, lr
	b trap_handle_error

	// Lower EL using AArch32 / SError
	.align 7

	mov	x0, lr
	b trap_handle_error


.macro kernel_entry
	sub		sp, sp, #S_FRAME_SIZE

	stp		x0, x1, [sp, #OFFSET_X0]
	stp		x2, x3, [sp, #OFFSET_X2]
	stp		x4, x5, [sp, #OFFSET_X4]
	stp		x6, x7, [sp, #OFFSET_X6]
	stp		x8, x9, [sp, #OFFSET_X8]
	stp		x10, x11, [sp, #OFFSET_X10]
	stp		x12, x13, [sp, #OFFSET_X12]
	stp		x14, x15, [sp, #OFFSET_X14]
	stp		x16, x17, [sp, #OFFSET_X16]
	stp		x18, x19, [sp, #OFFSET_X18]
	stp		x20, x21, [sp, #OFFSET_X20]
	stp		x22, x23, [sp, #OFFSET_X22]
	stp		x24, x25, [sp, #OFFSET_X24]
	stp		x26, x27, [sp, #OFFSET_X26]
	stp		x28, x29, [sp, #OFFSET_X28]
	str		lr, [sp, #OFFSET_LR]
.endm

.macro kernel_exit
	ldp		x0, x1, [sp, #OFFSET_X0]
	ldp		x2, x3, [sp, #OFFSET_X2]
	ldp		x4, x5, [sp, #OFFSET_X4]
	ldp		x6, x7, [sp, #OFFSET_X6]
	ldp		x8, x9, [sp, #OFFSET_X8]
	ldp		x10, x11, [sp, #OFFSET_X10]
	ldp		x12, x13, [sp, #OFFSET_X12]
	ldp		x14, x15, [sp, #OFFSET_X14]
	ldp		x16, x17, [sp, #OFFSET_X16]
	ldp		x18, x19, [sp, #OFFSET_X18]
	ldp		x20, x21, [sp, #OFFSET_X20]
	ldp		x22, x23, [sp, #OFFSET_X22]
	ldp		x24, x25, [sp, #OFFSET_X24]
	ldp		x26, x27, [sp, #OFFSET_X26]
	ldp		x28, x29, [sp, #OFFSET_X28]
	ldr		lr, [sp, #OFFSET_LR]

	add		sp, sp, #S_FRAME_SIZE
.endm

el0_sync_handler:

	kernel_entry

	mrs		x0, elr_el1
	str		x0, [sp, #OFFSET_PC]

	mrs		x0, sp_el0
	str		x0, [sp, #OFFSET_SP]

	mrs		x0, spsr_el1
	str		x0, [sp, #OFFSET_PSTATE]

	current_cpu	x10

	mov		x1, sp
	ldr 	x0, .LChypervisor_stack // Get the running hypervisor SVC stack
	ldr		x0, [x0, x10, lsl #3]

	mov		sp, x0
	curdom  x10, x12
	ldr		x12, [x10, #OFFSET_AVZ_SHARED]
	ldr		x10, [x12, #OFFSET_TRAPS_CALLBACK]

	// Come back on the domain stack
	mov		sp, x1

	cmp		x10, #0
	b.eq	out_sync

	// Make sure r0 refers to the base of the stack frame
	mov		x0, sp

    blr		x10

out_sync:

	ldr		x0, [sp, #OFFSET_PC]
	msr		elr_el1, x0

	ldr 	x0, [sp, #OFFSET_SP]
	msr		sp_el0, x0

	ldr		x0, [sp, #OFFSET_PSTATE]
	msr		spsr_el1, x0

	kernel_exit

	eret

.align  5
el01_irq_handler:

	kernel_entry

	mrs		x0, spsr_el1
	str		x0, [sp, #OFFSET_PSTATE]

	mrs		x0, sp_el0
	str		x0, [sp, #OFFSET_SP]

	mrs		x0, elr_el1
	str		x0, [sp, #OFFSET_PC]

	// Now process the IRQ differentiating execution according to the source (dom/hyp)
	// for managing the stack correctly

	current_cpu	x10

	ldr		x0, .LCpseudo_usr_mode
	ldr		x1, [x0, x10, lsl #3]
	cmp		x1, #0  				// svc ?

	b.eq	svc_path

	mov		x1, #0 					// setting svc
	str		x1, [x0, x10, lsl #3]

	mov		x1, sp
	ldr 	x0, .LChypervisor_stack // Get the running hypervisor SVC stack
	ldr		x0, [x0, x10, lsl #3]

	mov		sp, x0

	curdom	x0, x2
	str		x1, [x0, #OFFSET_G_SP]

	// Make sure r0 refers to the base of the stack frame
	mov		x0, sp

	str		lr, [sp, #-16]!
    bl 		irq_handle
	ldr		lr, [sp], #16

	curdom	x10, x11
	ldr		x11, [x10, #OFFSET_AVZ_SHARED]

	// If the softirq handling leads to trigger an interrupt in the guest,
	// it will be processed by do_evtchn_do_upcall. The way how to
	// process an interrupt with potentially IRQs off is under the
	// responsibility of the guest

	// are some IRQs pending?
	ldrb	w12, [x11, #OFFSET_EVTCHN_UPCALL_PENDING]
	tst		w12, #0xff

	b.eq	restore

	b		upcall_path

upcall_path:

	current_cpu x11

	curdom  x10, x12
	ldr		x12, [x10, #OFFSET_AVZ_SHARED]
	ldr		lr, [x12, #OFFSET_HYPERVISOR_CALLBACK]

	cmp		lr, #0
	beq		restore

	ldr		x0, .LChypervisor_stack   	// running L1 hypervisor stack
	mov		x12, sp
	str		x12, [x0, x11, lsl #3]

	// Get guest stack
	ldr		x12, [x10, #OFFSET_G_SP]
	mov		sp, x12

	// setting pseudo_usr_mode / x0, x1 re-assigned right after
	ldr 	x0, .LCpseudo_usr_mode
	mov		x1, #1
	str		x1, [x0, x11, lsl #3]

	// x0 contains a reference to the stack pointer
	mov		x0, sp

	// Go up to the guest for evtchn processing
	br		lr

restore:

	current_cpu x11

	// Setting pseudo_usr_mode / x0, x1 re-assigned right after
	ldr 	x0, .LCpseudo_usr_mode
	mov		x1, #1
	str		x1, [x0, x11, lsl #3]

	// Restore saved registers

	ldr		x0, .LChypervisor_stack   	// running SVC hypervisor stack
	mov		x10, sp
	str		x10, [x0, x11, lsl #3]

	curdom	x10, x11

	// Get guest stack
	ldr		x11, [x10, #OFFSET_G_SP]
	mov		sp, x11

out_irq:

	ldr		x0, [sp, #OFFSET_PSTATE]
	msr		spsr_el1, x0

	ldr		x0, [sp, #OFFSET_SP]
	msr		sp_el0, x0

	ldr		x0, [sp, #OFFSET_PC]
	msr		elr_el1, x0

  	kernel_exit

	eret

/*
 * This function is called at bootstrap and
 * reboot time. It initializes some registers
 */
ENTRY(pre_ret_to_user)

	// Initial state - IRQs off
	disable_irq

	// Get a reference to our domain descriptor
	curdom	x10, x11
	ldr		x11, [x10, #OFFSET_AVZ_SHARED]
	str		xzr, [x11, #OFFSET_HYPERVISOR_CALLBACK]

	current_cpu 	x11

	// Switch to the guest stack
	ldr 	x0, .LChypervisor_stack
	mov		x12, sp
	str		x12, [x0, x11, lsl #3]

	// Setting pseudo_usr_mode
	ldr 	x0, .LCpseudo_usr_mode
	mov		x1, #1
	str		x1, [x0, x11, lsl #3]

	ldr		x2, [sp, #OFFSET_PC]  // Entry point of the guest
	ldr		x21, [sp, #OFFSET_X21] // Device tree (fdt_addr)
	ldr		x22, [sp, #OFFSET_X22] // Address of start_info

	// Ready to jump into the Linux domain...
	blr		x2

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

	b upcall_path


pseudo_usr_mode:
	.space NR_CPUS * 8

// Hypervisor stack is used for the *current* (running) vcpu svc stack address
svc_path:

	mov		x0, sp

	// The stack must stay 16-byte aligned

	str		lr, [sp, #-16]!

    bl 		irq_handle
	ldr		lr, [sp], #16

    b 		out_irq

hypervisor_stack:
	.space NR_CPUS * 8


.LCpseudo_usr_mode:
	.quad	pseudo_usr_mode

.LChypervisor_stack:
	.quad	hypervisor_stack

