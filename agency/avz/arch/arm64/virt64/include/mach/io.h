/*
 * Copyright (C) 2020-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef MACH_IO
#define MACH_IO

/* General I/O space */
#define IO_SPACE_PADDR	0x8000000
#define IO_SPACE_SIZE	0x3000000

/* UART */
#define UART_BASE 0x09000000

/* GIC */
#define GIC_DIST_PHYS 	0x08000000
#define GIC_DIST_SIZE   0x10000

#define GIC_CPU_PHYS 	0x08010000
#define GIC_CPU_SIZE	0x10000

#define GIC_HYP_PHYS	0x08030000
#define GIC_HYP_SIZE	0x10000


#endif /* MACH_IO */

