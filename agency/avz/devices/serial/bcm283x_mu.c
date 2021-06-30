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
#include <memory.h>

#include <device/serial.h>

#include <device/arch/bcm283x_mu.h>
#include <device/arch/pl011.h>

#include <mach/uart.h>

#include <asm/io.h>

void *__uart_vaddr = (void *) UART_BASE;

void *__uart5_vaddr = NULL;

int printch(char c) {

	bcm283x_mu_t *bcm283x_mu = (bcm283x_mu_t *) __uart_vaddr;

	/* We check if UART5 is used instead of UART1, typically if UART1
	 * is used with a LoRA module or other.
	 */
	if (!__uart5_vaddr && (__uart_vaddr != (void *) UART_BASE)) {
		__uart5_vaddr = (void *) io_map(RPI4_UART5_ADDR, PAGE_SIZE);
		BUG_ON(!__uart5_vaddr);
	}

	if (__uart5_vaddr && ioread32(__uart5_vaddr + RPI_UART_CR) & RPI_UART_EN) {

		while ((ioread16(((addr_t) __uart5_vaddr) + UART01x_FR) & UART01x_FR_TXFF)) ;

		iowrite16(((addr_t) __uart5_vaddr) + UART01x_DR, c);

		if (c == '\n') {
			while ((ioread16(((addr_t) __uart5_vaddr) + UART01x_FR) & UART01x_FR_TXFF)) ;
			iowrite16(((addr_t) __uart5_vaddr) + UART01x_DR, '\r'); /* Carriage return */

		}

	} else {


		/* Wait until there is space in the FIFO */
		while (!(ioread32(&bcm283x_mu->lsr) & UART_LSR_TX_READY)) ;

		/* Send the character */
		iowrite32(&bcm283x_mu->io, (uint32_t) c);

		if (c == '\n') {
			while (!(ioread32(&bcm283x_mu->lsr) & UART_LSR_TX_READY)) ;
			iowrite8(&bcm283x_mu->io, '\r');	/* Carriage return */
		}
	}

	return 1;
}


