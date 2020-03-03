/*
 *  linux/include/asm-arm/mach/irq.h
 *
 *  Copyright (C) 1995-2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_MACH_IRQ_H
#define __ASM_ARM_MACH_IRQ_H



#include <asm/irq.h>

#define do_level_IRQ	handle_level_irq
#define do_edge_IRQ	handle_edge_irq
#define do_simple_IRQ	handle_simple_irq
#define do_bad_IRQ	handle_bad_irq

int irq_set_affinity(unsigned int irq, struct cpumask *mask_val);

#endif
