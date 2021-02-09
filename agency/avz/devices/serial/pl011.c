/*
 * Copyright (C) 2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <types.h>

#include <device/arch/pl011.h>

#include <mach/uart.h>

#include <asm/io.h>

void *__uart_vaddr = (void *) UART_BASE;

int printch(char c) {

	while ((ioread16(((uint32_t) __uart_vaddr) + UART01x_FR) & UART01x_FR_TXFF)) ;

	iowrite16(((uint32_t) __uart_vaddr) + UART01x_DR, c);

	return 1;
}



