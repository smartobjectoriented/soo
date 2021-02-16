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

@ Manage various context-related code (context switch)


#include <generated/asm-offsets.h>

#include <asm/processor.h>
#include <asm/mmu.h>

.global __mmu_switch
.global cpu_do_idle


@ Switch the MMU to a L1 page table
@ r0 contains the full TTBR0 contents related to this CPU for the L1 page table
@ r1 is used as scratch registers.

__mmu_switch:

	dsb
	isb

	@ So far, we do not use ASID/Context ID

	mcr	p15, 0, r0, c2, c0, 0		@ set TTBR0

	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0		@ I+BTB cache invalidate

	dsb
	isb

	mov		pc, lr

cpu_do_idle:
	dsb					@ WFI may enter a low-power mode
	wfi
	mov pc,	lr
