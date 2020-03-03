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

#include <generated/asm-offsets.h>
#include <asm/asm-macros.h>
#include <asm/init.h>
#include <asm/linkage.h>

#define ATAG_CORE 0x54410001
#define ATAG_CORE_SIZE ((2*4 + 3*4) >> 2)
#define ATAG_CORE_SIZE_EMPTY ((2*4) >> 2)

#ifdef CONFIG_CPU_BIG_ENDIAN
#define OF_DT_MAGIC 0xd00dfeed
#else
#define OF_DT_MAGIC 0xedfe0dd0 /* 0xd00dfeed in big-endian */
#endif


	.type	__switch_data, %object
__switch_data:
	.long	__mmap_switched
	.long	__data_loc			@ r4
	.long	__data_start			@ r5
	.long	__bss_start			@ r6
	.long	_end				@ r7
	.long	processor_id			@ r4
	.long	__machine_arch_type		@ r5
	.long	fdt_paddr			@ r6
	.long	cr_alignment			@ r7
	.long 	cpu0_stack				@ This is sp which refers to a private area (cf botton of this file)

/*
 * The following fragment of code is executed with the MMU on in MMU mode,
 * and uses absolute addresses; this is not position independent.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r9  = processor ID
 */

	.type	__mmap_switched, %function
__mmap_switched:
	adr	r3, __switch_data + 4

	ldmia	r3!, {r4, r5, r6, r7}
	cmp	r4, r5				@ Copy data segment if needed
1:	cmpne	r5, r6
	ldrne	fp, [r4], #4
	strne	fp, [r5], #4
	bne	1b

	mov	fp, #0				@ Clear BSS (and zero fp)
1:	cmp	r6, r7
	strcc	fp, [r6],#4
	bcc	1b

	ldmia	r3, {r4, r5, r6, r7, sp}
	str	r9, [r4]			@ Save processor ID
	str	r1, [r5]			@ Save machine type
	str	r2, [r6]			@ Save atags pointer
	bic	r4, r0, #CR_A			@ Clear 'A' bit
	stmia	r7, {r0, r4}			@ Save control register values

	b __start_avz


__error_p:
#ifdef CONFIG_DEBUG_LL
    adr r0, str_p1
    bl  printascii
    mov r0, r9
    bl  printhex8
    adr r0, str_p2
    bl  printascii
    b   __error
str_p1: .asciz  "\nError: unrecognized/unsupported processor variant (0x"
str_p2: .asciz  ").\n"
    .align
#endif
ENDPROC(__error_p)


	.type	__error, %function
__error:

1:	mov	r0, r0
	b	1b

__CPUINIT

/*
 * Read processor ID register (CP#15, CR0), and look up in the linker-built
 * supported processor list.  Note that we can't use the absolute addresses
 * for the __proc_info lists since we aren't running with the MMU on
 * (and therefore, we are not in the correct address space).  We have to
 * calculate the offset.
 *
 *	r9 = cpuid
 * Returns:
 *	r3, r4, r6 corrupted
 *	r5 = proc_info pointer in physical address space
 *	r9 = cpuid (preserved)
 */
	.type	__lookup_processor_type, %function
__lookup_processor_type:
	adr	r3, 3f
	ldmda	r3, {r5 - r7}
	sub	r3, r3, r7			@ get offset between virt&phys
	add	r5, r5, r3			@ convert virt addresses to
	add	r6, r6, r3			@ physical address space
1:	ldmia	r5, {r3, r4}			@ value, mask
	and	r4, r4, r9			@ mask wanted bits
	teq	r3, r4
	beq	2f
	add	r5, r5, #PROC_INFO_SZ		@ sizeof(proc_info_list)
	cmp	r5, r6
	blo	1b
	mov	r5, #0				@ unknown processor
2:	mov	pc, lr

/*
 * This provides a C-API version of the above function.
 */
ENTRY(lookup_processor_type)
	stmfd	sp!, {r4 - r7, r9, lr}
	mov	r9, r0
	bl	__lookup_processor_type
	mov	r0, r5
	ldmfd	sp!, {r4 - r7, r9, pc}

/*
 * Look in include/asm-arm/procinfo.h and arch/arm/kernel/arch.[ch] for
 * more information about the __proc_info and __arch_info structures.
 */
	.long	__proc_info_begin
	.long	__proc_info_end
3:	.long	.
	.long	__arch_info_begin
	.long	__arch_info_end

/*
 * Lookup machine architecture in the linker-build list of architectures.
 * Note that we can't use the absolute addresses for the __arch_info
 * lists since we aren't running with the MMU on (and therefore, we are
 * not in the correct address space).  We have to calculate the offset.
 *
 *  r1 = machine architecture number
 * Returns:
 *  r3, r4, r6 corrupted
 *  r5 = mach_info pointer in physical address space
 */
	.type	__lookup_machine_type, %function
__lookup_machine_type:
	adr	r3, 3b
	ldmia	r3, {r4, r5, r6}
	sub	r3, r3, r4			@ get offset between virt&phys
	add	r5, r5, r3			@ convert virt addresses to
	add	r6, r6, r3			@ physical address space
1:	ldr	r3, [r5, #MACHINFO_TYPE]	@ get machine type
	teq	r3, r1				@ matches loader number?
	beq	2f				@ found
	add	r5, r5, #SIZEOF_MACHINE_DESC	@ next machine_desc
	cmp	r5, r6
	blo	1b
	mov	r5, #0				@ unknown machine
2:	mov	pc, lr

/*
 * This provides a C-API version of the above function.
 */
ENTRY(lookup_machine_type)
	stmfd	sp!, {r4 - r6, lr}
	mov	r1, r0
	bl	__lookup_machine_type
	mov	r0, r5
	ldmfd	sp!, {r4 - r6, pc}


/* Determine validity of the r2 atags pointer.  The heuristic requires
 * that the pointer be aligned, in the first 16k of physical RAM and
 * that the ATAG_CORE marker is first and present.  Future revisions
 * of this function may be more lenient with the physical address and
 * may also be able to move the ATAGS block if necessary.
 *
 * r8  = machinfo
 *
 * Returns:
 *  r2 either valid atags pointer, or zero
 *  r5, r6 corrupted
 */

	.type	__vet_atags, %function
__vet_atags:
	tst	r2, #0x3			@ aligned?
	bne	1f
    ldr r5, [r2, #0]
#if 1
    ldr r6, =OF_DT_MAGIC        @ is it a DTB?
    cmp r5, r6
    beq 2f
#endif
    cmp r5, #ATAG_CORE_SIZE     @ is first tag ATAG_CORE?
    cmpne   r5, #ATAG_CORE_SIZE_EMPTY
    bne 1f
    ldr r5, [r2, #4]
    ldr r6, =ATAG_CORE
    cmp r5, r6
    bne 1f

2:	mov	pc, lr				@ atag pointer is ok

1:	mov	r2, #0
	mov	pc, lr


.section	".init_task", "w"

@ The following stacks will be aligned on 8 KB (13-bits address) since
@ retrieving the current domain (macro current()) is and'ing on the least significant 13 bits
@ to retrieve the cpu_info structure (stored at the top of the stack, i.e. bottom address)
.align 13

cpu0_stack_bottom:
    .fill STACK_SIZE, 1, 0
cpu0_stack:

.global cpu1_stack
.global cpu2_stack
.global cpu3_stack

.skip 8
.align 13
cpu1_stack_bottom:
    .fill STACK_SIZE, 1, 0
cpu1_stack:

.skip 8
.align 13
cpu2_stack_bottom:
    .fill STACK_SIZE, 1, 0
cpu2_stack:

.skip 8
.align 13
cpu3_stack_bottom:
    .fill STACK_SIZE, 1, 0
cpu3_stack:



.section	"._end"
_vmlinux_end:


