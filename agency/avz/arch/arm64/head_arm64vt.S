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
#include <linkage.h>

#include <device/arch/gic.h>

#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/image.h>
#include <asm/virt.h>

.global __start
.global __fdt_addr
.global __mmu_setup

.extern clear_bss
.extern mmu_configure
.extern __vectors

.extern __cpu0_stack

.section ".head.text","ax"

__pre_head:
	b	__start					// branch to the main entry point

	.long	0					// reserved

	.quad	0					// Image load offset from start of RAM, little-endian
	.quad	__end - __start		// Effective size of kernel image, little-endian
	.quad	__HEAD_FLAGS		// Informative flags, little-endian
	.quad	0					// reserved
	.quad	0					// reserved
	.quad	__end				// reserved
	.ascii	__HEAD_MAGIC		// Magic number
	.long	0					// reserved

__start:

	// Preserve the fdt addr (device tree) which is stored in x0 by U-boot
	mov		x9, x0

	// We assume that U-boot started us in EL2 hyp mode

	bl		el2_setup

	/* Clear the BSS */

	adrp	x0, __bss_start
	adrp	x1, __bss_end
1:
	strb	wzr, [x0], #1

	cmp		x0, x1
	b.cc	1b

	// Initialize the vector table
	ldr		x0, =__vectors
	msr 	vbar_el2, x0

	// Set up the MMU
    b		mmu_setup

__kernel_main:

	// C main entry point
	b 		kernel_start

	// never returns...


.align 2

__mmu_setup:
		// The code to configure tcr_el2
		// is borrowed from jailhouse hypervisor
	
		mrs		x1, tcr_el2
	
		/*
		 * set TCR.(I)PS to the highest supported ID_AA64MMFR0_EL1.PARange value
		 */

		mrs     x2, id_aa64mmfr0_el1

		/* Narrow PARange to fit the PS field in TCR_ELx */
		ubfx    x2, x2, #ID_AA64MMFR0_PARANGE_SHIFT, #3
		bfi     x1, x2, #TCR_PS_SHIFT, #3

		msr		tcr_el2, x1

		/*
		 * The state of the TLBs is unknown before turning on the MMU.
		 * Flush them to avoid stale one.
		 */
		tlbi  alle2                  /* Flush hypervisor TLBs */
		dsb   nsh

		msr   ttbr0_el2, x0
		isb

		ldr	x0, =(SCTLR_I_BIT | SCTLR_C_BIT | SCTLR_M_BIT | SCTLR_EL2_RES1)

		dsb   sy                     /* Flush PTE writes and finish reads */
		msr   sctlr_el2, x0          /* now paging is enabled */
		isb                          /* Now, flush the icache */

		ret

mmu_setup:

		// Use a temporary stack
		adrp	x0, temp_stack
		mov		sp, x0

		mov		x0, x9 // fdt addr

		bl 		mmu_configure

		// Readjust the stack
		ldr		x0, =__cpu0_stack
		mov		sp, x0

		// Keep executing in the kernel space

		// Store the virtual address which will be used to continue
		// the execution after the MMU enabled.
		ldr		x0, .LCvirt_entry

		blr		x0

el2_setup:

		/* Populate ID registers. */
		mrs	x0, midr_el1
		mrs	x1, mpidr_el1
		msr	vpidr_el2, x0
		msr	vmpidr_el2, x1

		msr	sctlr_el1, xzr

		/* Hyp configuration. */
	    and x1, x1, #3

		cmp x1, #AGENCY_RT_CPU
		bgt ME

agency:
	ldr x0, =HCR_AGENCY_FLAGS
	b	99f
ME:
	ldr x0, =HCR_ME_FLAGS
99:

	msr	hcr_el2, x0
	isb

	msr	cntvoff_el2, xzr		// Clear virtual offset

    /*
     * Ensure that any exceptions encountered at EL2
     * are handled using the EL2 stack pointer, rather
     * than SP_EL0.
    */
    msr spsel, #1

	ret


ENTRY(secondary_startup)
	/*
     * Common entry point for secondary CPUs.
     *
     * Ensure that we're in SVC mode, and IRQs are disabled.  Lookup
     * the processor type - there is no need to check the machine type
     * as it has already been validated by the primary processor.
     */

	bl		el2_setup

	// Initialize the vector table
	ldr		x0, =__vectors
	msr 	vbar_el2, x0

    adr 	x4, __secondary_data

	ldr		x5, [x4]
	ldr		x19, [x4, #8]

	// Nice trick to retrieve secondary_data field values (pgdir, stack)
	// x5 is a virtual addresses at same location than x4 containing the physical location.
	// So x10 will contain the offset
	sub		x10, x4, x5

	// Get secondary_data.pgdir (x19 is virtual, x0 is physical)
	ldr		x0, [x10, x19]

	// Get the stack pointer (virt. address)
	add		x19, x19, #8
	ldr		x8, [x10, x19]
	mov		sp, x8

	// Convert to physical address
	add		sp, sp, x10

	bl		mmu_configure

	ldr		x8, [x19]
	mov		sp, x8

  	// Keep executing in the kernel space

  	// Store the virtual address which will be used to continue
    // execution after the MMU enabled.
  	ldr		x19, .LC_virt_secondary_entry

  	blr		x19

    .type   __secondary_data, %object
__secondary_data:
    .quad   .
    .quad   secondary_data

.ltorg

__fdt_addr:
  .quad 	0

.LCvirt_entry:
  .quad __kernel_main

.LC_virt_secondary_entry:
  .quad secondary_start_kernel

.align 13

	// Before MMU is enabled, we cannot refer to the normal stack as declared in the linker script

	.space 4096
temp_stack:

