/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef MACH_UART_H
#define MACH_UART_H

/* UART1 (mini UART) */
#define RPI4_UART1_ADDR		0xfe215040

/* UART5 (pl011) */
#define RPI4_UART5_ADDR 	0xfe201a00

/* Initial UART */
#define UART_BASE		RPI4_UART1_ADDR

#endif /* MACH_UART_H */

