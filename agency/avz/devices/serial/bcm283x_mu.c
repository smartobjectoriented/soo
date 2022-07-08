/*
 * Copyright (C) 2017 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <common.h>

#include <device/serial.h>

#include <device/arch/bcm283x_mu.h>
#include <mach/uart.h>

#include <asm/io.h>

void *__uart_vaddr = (void *) UART_BASE;

int printch(char c) {

	bcm283x_mu_t *bcm283x_mu = (bcm283x_mu_t *) __uart_vaddr;

	/* Wait until there is space in the FIFO */
	while (!(ioread32(&bcm283x_mu->lsr) & UART_LSR_TX_READY)) ;

	/* Send the character */
	iowrite32(&bcm283x_mu->io, (uint32_t) c);

	if (c == '\n') {
		while (!(ioread32(&bcm283x_mu->lsr) & UART_LSR_TX_READY)) ;
		iowrite32(&bcm283x_mu->io, (uint32_t ) '\r');	/* Carriage return */
	}

	return 1;
}


