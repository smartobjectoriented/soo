/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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


#include <mach/bcm2835.h>

#define CONFIG_DEBUG_UART_PHYS BCM2835_UART0_PHYS
#define CONFIG_DEBUG_UART_VIRT BCM2835_UART0_VIRT

#if 0
.macro __print
	ldr r0, =0xf8c15040
	ldr r1, ='!'

	str r1, [r0]
.endm
#endif
		.macro	addruart, rp, rv, tmp
		ldr	\rp, =CONFIG_DEBUG_UART_PHYS
		ldr	\rv, =CONFIG_DEBUG_UART_VIRT
		.endm

		.macro	senduart,rd,rx
		str		\rd, [\rx, #MU_IO_REG]
		.endm

		.macro	busyuart,rd,rx
		.endm

		.macro	waituart,rd,rx


1002:
 		ldr		\rd, [\rx, #MU_LSR_REG]
 		ands     \rd, \rd, #MU_LSR_TX_EMPTY

		beq	1002b

		.endm
