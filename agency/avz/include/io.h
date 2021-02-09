/*
 *  Copyright (C) 2016-2020 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Interrupt architecture for the GIC:
 *
 * o There is one Interrupt Distributor, which receives interrupts
 *   from system devices and sends them to the Interrupt Controllers.
 *
 * o There is one CPU Interface per CPU, which sends interrupts sent
 *   by the Distributor, and interrupts generated locally, to the
 *   associated CPU. The base address of the CPU interface is usually
 *   aliased so that the same address points to different chips depending
 *   on the CPU it is accessed from.
 *
 * Note that IRQs 0-31 are special - they are local to each CPU.
 * As such, the enable set/clear, pending set/clear and active bit
 * registers are banked per-cpu for these sources.
 */

#ifndef IO_H
#define IO_H

#define writeb(v,a)	(*(volatile unsigned char __force  *)(a) = (v))
#define writew(v,a)	(*(volatile unsigned short __force *)(a) = (v))
#define writel(v,a)	(*(volatile unsigned int __force   *)(a) = (v))

#define readb(a)		(*(volatile unsigned char __force  *)(a))
#define readw(a)		(*(volatile unsigned short __force *)(a))
#define readl(a)		(*(volatile unsigned int __force   *)(a))


#endif /* IO_H */
