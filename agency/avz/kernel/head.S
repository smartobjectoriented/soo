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

#include <asm/assembler.h>
#include <asm/linkage.h>
#include <avz/init.h>

#include <asm/domain.h>
#include <asm/procinfo.h>
#include <asm/ptrace.h>
#include <generated/asm-offsets.h>
#include <asm/asm-macros.h>
#include <asm/memory.h>
#include <asm/unified.h>
 
#include <asm/system.h>
 
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgtable-hwdef.h>
#include <asm/config.h>

#define KERNEL_RAM_ADDR (L_PAGE_OFFSET + 0x8000)

/*
 * swapper_pg_dir is the virtual address of the initial page table.
 * We place the page tables 16K below KERNEL_RAM_ADDR.  Therefore, we must
 * make sure that KERNEL_RAM_ADDR is correctly set.  Currently, we expect
 * the least significant 16 bits to be 0x8000, but we could probably
 * relax this restriction to KERNEL_RAM_ADDR >= PAGE_OFFSET + 0x4000.
 */
	.globl	swapper_pg_dir
	.equ	swapper_pg_dir, PAGE_OFFSET+0x4000

	.macro	pgtbl, rd
	@ldr	\rd, =(__virt_to_phys(KERNEL_RAM_ADDR - 0x4000))
	ldr	\rd, =(PHYS_OFFSET+0x4000)
	.endm

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code.  The requirements
 * are: MMU = off, D-cache = off, I-cache = dont care, r0 = 0,
 * r1 = machine nr, r2 = fdt_paddr.
 *
 * This code is mostly position independent, so if you link the kernel at
 * 0xc0008000, you call this at __pa(0xc0008000).
 *
 * See linux/arch/arm/tools/mach-types for the complete list of machine
 * numbers for r1.
 *
 * We're trying to keep crap to a minimum; DO NOT add any machine specific
 * crap here - that's what the boot loader (or in extreme, well justified
 * circumstances, zImage) is for.
 */

	__START

	.type	stext, %function
ENTRY(stext)

	bl	__hyp_stub_install

	@ ensure svc mode and all interrupts masked
	safe_svcmode_maskall r9

	mrc	p15, 0, r9, c0, c0		@ get processor id

	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?

	bl	__lookup_machine_type		@ r5=machinfo
	movs	r8, r5				@ invalid machine (r5=0)?


	bl	__vet_atags

	bl	__create_page_tables

        /*
	 * The following calls CPU specific code in a position independent
	 * manner.  See arch/arm/mm/proc-*.S for details.  r10 = base of
	 * xxx_proc_info structure selected by __lookup_processor_type
	 * above.  On return, the CPU will be ready for the MMU to be
	 * turned on, and r0 will hold the CPU control register value.
	 */
	ldr	r13, =__mmap_switched		@ address to jump to after
						@ mmu has been enabled
	adr	lr, 1f			@ return (PIC) address

	mov	r8, r4				@ set TTBR1 to swapper_pg_dir
	add	pc, r10, #PROCINFO_INITFUNC

1:	b	__enable_mmu
ENDPROC(stext)
	.ltorg
#ifndef CONFIG_XIP_KERNEL
2:	.long	.
	.long	PAGE_OFFSET
#endif

.extern printch

    __CPUINIT

ENTRY(secondary_startup)
    /*
     * Common entry point for secondary CPUs.
     *
     * Ensure that we're in SVC mode, and IRQs are disabled.  Lookup
     * the processor type - there is no need to check the machine type
     * as it has already been validated by the primary processor.
     */

    safe_svcmode_maskall r9

    mrc p15, 0, r9, c0, c0      @ get processor id
    bl  __lookup_processor_type

    movs    r10, r5             @ invalid processor?
    moveq   r0, #'p'            @ yes, error 'p'

    beq __error_p

    /*
     * Use the page tables supplied from  __cpu_up.
     */
    adr 	r4, __secondary_data
    ldmia   r4, {r5, r7, r12}       @ retrieve secondary_data field values (pgdir, stack)
    sub 	lr, r4, r5          	@ r4 is a virtual address (linked at this location)
    ldr 	r4, [lr, r7]            @ get secondary_data.pgdir
    mov		r8, r4					@ used to setup ttbr1 in proc_initfunc
    adr 	lr, BSYM(__enable_mmu)  @ return address
    mov 	r13, r12                @ new stack pointer

    add 	pc, r10, #PROCINFO_INITFUNC  @ initialise processor

ENDPROC(secondary_startup)

    /*
     * r6  = &secondary_data
     */
ENTRY(__secondary_switched)
    ldr sp, [r7, #4]            @ get secondary_data.stack (second field of secondary_data)
    mov fp, #0
    b   secondary_start_kernel
ENDPROC(__secondary_switched)

    .align

    .type   __secondary_data, %object
__secondary_data:
    .long   .
    .long   secondary_data
    .long   __secondary_switched


/*
 * Setup common bits before finally enabling the MMU.  Essentially
 * this is just loading the page table pointer and domain access
 * registers.
 */
	.type	__enable_mmu, %function
__enable_mmu:

#if defined(CONFIG_ALIGNMENT_TRAP) && __LINUX_ARM_ARCH__ < 6
	orr	r0, r0, #CR_A
#else
	bic	r0, r0, #CR_A
#endif

#ifdef CONFIG_ARM_LPAE
	mov	r5, #0
	mcrr	p15, 0, r4, r5, c2		@ load TTBR0
#else
	mov	r5, #(domain_val(DOMAIN_USER, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_TABLE, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_IO, DOMAIN_MANAGER))
	mcr	p15, 0, r5, c3, c0, 0		@ load domain access register
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
#endif
	b	__turn_mmu_on

/*
 * Enable the MMU.  This completely changes the structure of the visible
 * memory space.  You will not be able to trace execution through this.
 * If you have an enquiry about this, *please* check the linux-arm-kernel
 * mailing list archives BEFORE sending another post to the list.
 *
 *  r0  = cp#15 control register
 *  r13 = *virtual* address to jump to upon completion
 *
 * other registers depend on the function called upon completion
 */
	.align	5
	.pushsection	.idmap.text, "ax"
ENTRY(__turn_mmu_on)
	mov	r0, r0
	instr_sync
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
	mrc	p15, 0, r3, c0, c0, 0		@ read id reg
	instr_sync
	mov	r3, r3
	mov	r3, r13
	mov	pc, r3
__turn_mmu_on_end:
ENDPROC(__turn_mmu_on)
	.popsection

/*
 * Setup the initial page tables.  We only setup the barest
 * amount which are required to get the kernel running, which
 * generally means mapping in the kernel code.
 *
 * r8  = machinfo
 * r9  = cpuid
 * r10 = procinfo
 *
 * Returns:
 *  r0, r3, r6, r7, r11 corrupted
 *  r4 = physical page table address
 */
	.type	__create_page_tables, %function
__create_page_tables:
	pgtbl	r4				@ page table address

	/*
	 * Clear the 16K level 1 swapper page table
	 */
	mov	r0, r4
	mov	r3, #0
	add	r6, r0, #0x4000
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	teq	r0, r6
	bne	1b

	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] @ mm_mmuflags

	/*
	 * (DRE)
	 * Create identity mapping for all available RAM
	 */
	mov	r6, pc, lsr #20			@ start of kernel section
	orr	r3, r7, r6, lsl #20		@ flags + kernel base
	str	r3, [r4, r6, lsl #2]	@ identity mapping

	/*
	 * Now setup the pagetables for our kernel direct
	 * mapped region.
	 */
	add	r0, r4,  #(KERNEL_RAM_ADDR & 0xff000000) >> 18
	str	r3, [r0, #(KERNEL_RAM_ADDR & 0x00f00000) >> 18]!
	ldr	r6, =(L_PAGE_OFFSET + CONFIG_RAM_SIZE - 1)
	add	r0, r0, #4
	add	r6, r4, r6, lsr #18
1:	cmp	r0, r6
	add	r3, r3, #1 << 20
	strls	r3, [r0], #4
	bls	1b

#include <mach/debug-macro.S>

/*
 * Map in IO space for serial debugging.
 * This allows debug messages to be output
 * via a serial console before paging_init.
*/

   ldr	   r7, =CONFIG_DEBUG_UART_PHYS
   ldr	   r3, =CONFIG_DEBUG_UART_VIRT

   mov     r3, r3, lsr #20
   mov     r3, r3, lsl #2

   add     r0, r4, r3
   rsb     r3, r3, #0x4000                 @ PTRS_PER_PGD*sizeof(long)
   cmp     r3, #0x0800                     @ limit to 512MB
   movhi   r3, #0x0800
   add     r6, r0, r3
   mov     r3, r7, lsr #20
   ldr     r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
   orr     r3, r7, r3, lsl #20
1: str     r3, [r0], #4
   add     r3, r3, #1 << 20
   teq     r0, r6

@	bne	1b     @ Just mapping 1 Mo is enough in most cases (DRE)

@ Now writing the contents within the page table to map the hypervisor area onto the physical space
@ Note that we are doing this here right after the DEBUG_LL stuff.

setup_pgtable_hypervisor:
	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS]
	mov r3, #0

	mov r0, #HYPERVISOR_VIRT_START

	ldr r6, =PHYS_OFFSET 			@ HYPERVISOR_PHYS_START
	ldr r11, =(HYPERVISOR_SIZE >> 20)

	orr	r3, r3, r7

	b		2f

1:
	str		r6, [r4, r0]
	add		r0, r0, #4
	add		r6, r6, #0x100000
	adds	r11, r11, #-1
	bhi		1b

2:
	orr		r6, r6, r3

	mov 	r0, r0, lsr #18
	cmp		r11, #0
	bne		1b

	mov	pc, lr
	.ltorg


#include "head-common.S"
