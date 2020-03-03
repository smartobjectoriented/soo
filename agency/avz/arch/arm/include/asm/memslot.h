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
#ifndef __ASM_MEMMAP_HEADER
#define __ASM_MEMMAP_HEADER

#include <asm/page.h>
#include <mach/memory.h>
#include <avz/libelf.h>

#include <soo/uapi/soo.h>
#include <soo/soo.h>


/* Number of possible MEs in the local SOO */
#define MEMSLOT_BASE	  2
#define MEMSLOT_NR	  (MEMSLOT_BASE + MAX_ME_DOMAINS)

/* Basic memslots. */
#define MEMSLOT_AVZ	  0
#define MEMSLOT_AGENCY	  1

/*
 * Memslot management
 *
 * Describes how domains are mapped in physical memory
 */
typedef struct {
	unsigned long base_paddr;  /* Kernel physical start address */
	unsigned int size;
	unsigned int busy; /* Indicate if a memslot is available or not */

	unsigned int fdt_paddr; /* Device Tree */

} memslot_entry_t;

extern memslot_entry_t memslot[];

/**
 * We put all the guest domains in ELF format on top of memory so
 * that the domain_build will be able to elf-parse and load to their final destination.
 */
void loadAgency(void);
void loadME(unsigned int slotID, uint8_t *img, dtb_feat_t *dtb_feat);


#endif /* !__ASM_MEMMAP_HEADER */
