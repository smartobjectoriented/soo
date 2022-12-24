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

.global cpu_entrypoint

.global __vectors

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
	b 	trap_handle_error

	// Current EL with SP0 / IRQ
	.align 7

	mov	x0, lr
	b 	trap_handle_error

	// Current EL with SP0 / FIQ
	.align 7

	mov	x0, lr
	b 	trap_handle_error

	// Current EL with SP0 / SError
	.align 7

	mov	x0, lr
	b 	trap_handle_error

	// Current EL with SPx / Synchronous
	.align 7

	b	trap_handle_error

	// Current EL with SPx / IRQ
	.align 7

	b 	el12_irq_handler

	// Current EL with SPx / FIQ
	.align 7

	mov	x0, lr
	b 	trap_handle_error

	// Current EL with SPx / SError
	.align 7

	mov	x0, lr
	mrs	x1, esr_el1

	b	__sync_serror

	// Lower EL using AArch64 / Synchronous
	.align 7

	b el2_sync_handler

	// Lower EL using AArch64 / IRQ
	.align 7

	b 	el12_irq_handler

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

	mov	x0, lr
	b trap_handle_error

	// Lower EL using AArch32 / SError
	.align 7

	mov	x0, lr
	b trap_handle_error


el2_sync_handler:

	kernel_entry

	mrs		x0, spsr_el2
	str		x0, [sp, #OFFSET_PSTATE]

	mrs		x0, sp_el1
	str		x0, [sp, #OFFSET_SP]

	mrs		x0, elr_el2
	str		x0, [sp, #OFFSET_PC]

 	mov 	x0, sp
	bl		trap_handle

	str		x0, [sp, #OFFSET_X0]

	ldr		x0, [sp, #OFFSET_PSTATE]
	msr		spsr_el2, x0

	ldr		x0, [sp, #OFFSET_SP]
	msr		sp_el1, x0

	ldr		x0, [sp, #OFFSET_PC]
	msr		elr_el2, x0

	kernel_exit

	eret


el1_sync_handler:

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
el12_irq_handler:

	kernel_entry

	mrs		x0, spsr_el2
	str		x0, [sp, #OFFSET_PSTATE]

	mrs		x0, sp_el1
	str		x0, [sp, #OFFSET_SP]

	mrs		x0, elr_el2
	str		x0, [sp, #OFFSET_PC]

	// Make sure r0 refers to the base of the stack frame
	mov		x0, sp

	str		lr, [sp, #-16]!
    bl 		irq_handle
	ldr		lr, [sp], #16

out_irq:

	ldr		x0, [sp, #OFFSET_PSTATE]
	msr		spsr_el2, x0

	ldr		x0, [sp, #OFFSET_SP]
	msr		sp_el1, x0

	ldr		x0, [sp, #OFFSET_PC]
	msr		elr_el2, x0

  	kernel_exit

	eret

ENTRY(pre_ret_to_el1)

	b entry_el1

	// Prepare to move towards EL1
	adr		x2, entry_el1
	msr		elr_el2, x2

	// Set the CPU in EL1 mode to proceed with
	// the bootstrap of the domain

	mov		x2, #PSR_MODE_EL1t
	msr		spsr_el2, x2

	// Ready to jump into the Linux domain...
	eret

	/*
	 * Mitigate Straight-line Speculation.
	 * Guard against Speculating past an ERET instruction and
	 * potentially perform speculative accesses to memory before
	 * processing the exception return
	 */
	dsb nsh
	isb

entry_el1:

	wfi

	ldr 	x0, cpu_entrypoint
	msr		elr_el2, x0

	// Set the CPU in EL1 mode to proceed with
	// the bootstrap of the domain

	mov		x2, #PSR_MODE_EL1t

	// Make sure no interrupt coming from CPU #0 is
	// interferring with other CPU bootstrap
	orr		x2, x2, #PSR_I_BIT

	msr		spsr_el2, x2

	// According to boot protocol
	mov		x1, #0
	mov		x1, #0
	mov		x2, #0
	mov		x3, #0

	// Ready to jump into the Linux domain...

	eret

	/*
	 * Mitigate Straight-line Speculation.
	 * Guard against Speculating past an ERET instruction and
	 * potentially perform speculative accesses to memory before
	 * processing the exception return
	 */
	dsb nsh
	isb

	nop
	nop
	nop

cpu_entrypoint:
	.quad 0x0

/*
 * This function is called at bootstrap and
 * reboot time. It initializes some registers
 */
ENTRY(pre_ret_to_user)

	// Initial state - IRQs off
	disable_irq

	// Get a reference to our domain descriptor
	curdom	x10, x11

	ldr		x2, [sp, #OFFSET_PC]  // Entry point of the guest
	msr		elr_el2, x2

	// Set the CPU in EL1 mode to proceed with
	// the bootstrap of the domain

	mov		x2, #PSR_MODE_EL1t
	msr		spsr_el2, x2

	ldr		x0, [sp, #OFFSET_X21] // Device tree (fdt_addr)

	// According to boot protocol
	mov		x1, #0
	mov		x2, #0
	mov		x3, #0

	// Ready to jump into the Linux domain...

	eret

	/*
	 * Mitigate Straight-line Speculation.
	 * Guard against Speculating past an ERET instruction and
	 * potentially perform speculative accesses to memory before
	 * processing the exception return
	 */
	dsb nsh
	isb

__crash:
	b __crash

#if 0
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

// Hypervisor stack is used for the *current* (running) vcpu svc stack address
svc_path:

	mov		x0, sp

	// The stack must stay 16-byte aligned

	str		lr, [sp, #-16]!

    bl 		irq_handle
	ldr		lr, [sp], #16

    b 		out_irq
#endif

hypervisor_stack:
	.space NR_CPUS * 8

.LChypervisor_stack:
	.quad	hypervisor_stack

