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


#ifndef A64_H
#define A64_H

#define SUNXI_GIC_DIST_PHYS 	0x01c81000
#define SUNXI_GIC_DIST_SIZE 	0x1000

#define SUNXI_GIC_CPU_PHYS 	0x01c82000
#define SUNXI_GIC_CPU_SIZE 	0x1000

#define SUNXI_UART0_VIRT	0xf8c28000
#define SUNXI_UART0_PHYS	0x01c28000

#define SUNXI_TIMER0		0x01c20c00
#define SUNXI_TIMER_HS0		0x01c60000

#define IRQ_SUNXI_TIMER0	(32 + 18)
#define IRQ_SUNXI_TIMER1	(32 + 19)
#define IRQ_SUNXI_HS_TIMER0	(32 + 81)

#define SUNXI_CPUCFG_PHYS	0x01c25c00
#define SUNXI_CPUCFG_SIZE	0x400

#define SUNXI_PMU_PHYS		0x01c25400
#define SUNXI_PMU_SIZE		0x400

extern struct smp_operations    sun50i_smp_ops;



#endif /* A64_H */
