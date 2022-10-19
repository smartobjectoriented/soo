/*
 *  linux/include/asm-arm/hardware/gic.h
 *
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_HARDWARE_GIC_H
#define __ASM_ARM_HARDWARE_GIC_H

#define ICC_SRE_EL2_SRE			(1 << 0)
#define ICC_SRE_EL2_ENABLE		(1 << 3)

#define GIC_CPU_CTRL			0x00
#define GIC_CPU_PRIMASK			0x04
#define GIC_CPU_BINPOINT		0x08
#define GIC_CPU_INTACK			0x0c
#define GIC_CPU_EOI			0x10
#define GIC_CPU_RUNNINGPRI		0x14
#define GIC_CPU_HIGHPRI			0x18
#define GIC_CPU_ALIAS_BINPOINT		0x1c
#define GIC_CPU_ACTIVEPRIO		0xd0
#define GIC_CPU_IDENT			0xfc

#define GICC_ENABLE			0x1
#define GICC_INT_PRI_THRESHOLD		0xf0
#define GICC_IAR_INT_ID_MASK		0x3ff
#define GICC_DIS_BYPASS_MASK		0x1e0

#define GIC_DIST_CTRL			0x000
#define GIC_DIST_CTR			0x004
#define GIC_DIST_IGROUP			0x080
#define GIC_DIST_ENABLE_SET		0x100
#define GIC_DIST_ENABLE_CLEAR		0x180
#define GIC_DIST_PENDING_SET		0x200
#define GIC_DIST_PENDING_CLEAR		0x280
#define GIC_DIST_ACTIVE_SET		0x300
#define GIC_DIST_ACTIVE_CLEAR		0x380
#define GIC_DIST_PRI			0x400
#define GIC_DIST_TARGET			0x800
#define GIC_DIST_CONFIG			0xc00
#define GIC_DIST_SOFTINT		0xf00
#define GIC_DIST_SGI_PENDING_CLEAR	0xf10
#define GIC_DIST_SGI_PENDING_SET	0xf20

#define GICD_ENABLE			0x1
#define GICD_DISABLE			0x0
#define GICD_INT_ACTLOW_LVLTRIG		0x0
#define GICD_INT_EN_CLR_X32		0xffffffff
#define GICD_INT_EN_SET_SGI		0x0000ffff
#define GICD_INT_EN_CLR_PPI		0xffff0000
#define GICD_INT_DEF_PRI		0xa0
#define GICD_INT_DEF_PRI_X4		((GICD_INT_DEF_PRI << 24) |\
					(GICD_INT_DEF_PRI << 16) |\
					(GICD_INT_DEF_PRI << 8) |\
					GICD_INT_DEF_PRI)

#define GICC_SIZE		0x2000
#define GICH_SIZE		0x2000

#define GICDv2_CIDR0		0xff0
#define GICDv2_PIDR0		0xfe0
#define GICDv2_PIDR2		0xfe8
#define GICDv2_PIDR4		0xfd0

#define GICC_CTLR		0x0000
#define GICC_PMR		0x0004
#define GICC_IAR		0x000c
#define GICC_EOIR		0x0010
#define GICC_DIR		0x1000

#define GICC_CTLR_GRPEN1	(1 << 0)
#define GICC_CTLR_EOImode	(1 << 9)

#define GICC_PMR_DEFAULT	0xf0

#define GICH_HCR		0x000
#define GICH_VTR		0x004
#define GICH_VMCR		0x008
#define GICH_ELSR0		0x030
#define GICH_ELSR1		0x034
#define GICH_APR		0x0f0
#define GICH_LR_BASE		0x100

#define GICV_PMR_SHIFT		3
#define GICH_VMCR_PMR_SHIFT	27
#define GICH_VMCR_EN0		(1 << 0)
#define GICH_VMCR_EN1		(1 << 1)
#define GICH_VMCR_ACKCtl	(1 << 2)
#define GICH_VMCR_EOImode	(1 << 9)

#define GICH_HCR_EN		(1 << 0)
#define GICH_HCR_UIE		(1 << 1)
#define GICH_HCR_LRENPIE	(1 << 2)
#define GICH_HCR_NPIE		(1 << 3)
#define GICH_HCR_VGRP0EIE	(1 << 4)
#define GICH_HCR_VGRP0DIE	(1 << 5)
#define GICH_HCR_VGRP1EIE	(1 << 6)
#define GICH_HCR_VGRP1DIE	(1 << 7)
#define GICH_HCR_EOICOUNT_SHIFT	27

#define GICH_LR_HW_BIT		(1 << 31)
#define GICH_LR_GRP1_BIT	(1 << 30)
#define GICH_LR_ACTIVE_BIT	(1 << 29)
#define GICH_LR_PENDING_BIT	(1 << 28)
#define GICH_LR_PRIORITY_SHIFT	23
#define GICH_LR_SGI_EOI_BIT	(1 << 19)
#define GICH_LR_CPUID_SHIFT	10
#define GICH_LR_PHYS_ID_SHIFT	10
#define GICH_LR_VIRT_ID_MASK	0x3ff

#define GIC_SGI_UNKNOWN			0
#define GIC_SGI_EVENT			1

#ifndef __ASSEMBLY__

extern void *gic_cpu_base_addr;

void gic_init(addr_t *, addr_t *, addr_t *);
void gicc_init(void);
void gic_raise_softirq(int cpu, unsigned int irq);

void init_gic(void);

#endif

#endif
