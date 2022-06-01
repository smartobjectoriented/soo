/*
 *  Copyright (C) 2016-2020 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef IO_H
#define IO_H

#define writeb(v,a)	(*(volatile unsigned char __force  *)(a) = (v))
#define writes(v,a)	(*(volatile unsigned short __force *)(a) = (v))
#define writei(v,a)	(*(volatile unsigned int __force *)(a) = (v))
#define writel(v,a)	(*(volatile unsigned long __force   *)(a) = (v))

#define readb(a)		(*(volatile unsigned char __force  *)(a))
#define reads(a)		(*(volatile unsigned short __force *)(a))
#define readi(a)		(*(volatile unsigned int __force *)(a))
#define readl(a)		(*(volatile unsigned long __force   *)(a))


#endif /* IO_H */
