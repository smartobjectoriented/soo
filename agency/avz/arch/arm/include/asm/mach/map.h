/*
 *  linux/include/asm-arm/map.h
 *
 *  Copyright (C) 1999-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Page table mapping constructs and function prototypes
 */

#ifndef MACH_MAP_H
#define MACH_MAP_H

#include <asm/page.h>

struct map_desc {
	unsigned long virtual;
	unsigned long pfn;
	unsigned long length;
	unsigned int type;
};

enum {
	MT_DEVICE,
	MT_DEVICE_NONSHARED,
	MT_DEVICE_CACHED,
	MT_DEVICE_WC,

  MT_UNCACHED = 4,
  MT_CACHECLEAN,
  MT_MINICLEAN,
  MT_LOW_VECTORS,
  MT_HIGH_VECTORS,
  MT_MEMORY_RWX,
  MT_MEMORY_RW,
  MT_ROM,
  MT_MEMORY_RWX_NONCACHED,
  MT_MEMORY_RW_DTCM,
  MT_MEMORY_RWX_ITCM,
  MT_MEMORY_RW_SO,
  MT_MEMORY_DMA_READY,
};


#define MT_NONSHARED_DEVICE     MT_DEVICE_NONSHARED

extern void iotable_init(struct map_desc *, int);
extern void create_mapping(struct map_desc *md, pde_t *pgd);


#endif /* MACH_MAP_H */
