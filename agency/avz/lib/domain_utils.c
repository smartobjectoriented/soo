/*
 * Copyright (C) 2016-2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <fdt_support.h>
#include <memslot.h>
#include <sched.h>

#include <lib/image.h>

#include <soo/soo.h>
#include <soo/uapi/avz.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#define L_TEXT_OFFSET	0x8000

extern unsigned int fdt_paddr; /* defined in kernel/setup.c */

/**
 * We put all the guest domains in ELF format on top of memory so
 * that the domain_build will be able to elf-parse and load to their final destination.
 */
void loadAgency(void)
{
	void *fdt;
	uint32_t dom_addr;

	/* Get the address of the device tree (FDT) passed by U-boot
	 * and configure the corresponding memslot (slot 1) of the Agency.
	 */
	fdt = (void *) __lva(fdt_paddr);

	dom_addr = fdt_getprop_u32_default(fdt, "/fit-images/agency", "load-addr", 0);

	/* Set the memslot base address to a section boundary */
	memslot[MEMSLOT_AGENCY].base_paddr = (dom_addr & ~(SZ_1M - 1));
	memslot[MEMSLOT_AGENCY].fdt_paddr = fdt_paddr;
	memslot[MEMSLOT_AGENCY].size = fdt_getprop_u32_default(fdt, "/agency", "domain-size", 0);
}

/*
 * The concatened image must be out of domains because of elf parser
 *
 * <img> represents the original binary image as injected in the user application
 * <target_dom> represents the target memory area
 */
void loadME(unsigned int slotID, uint8_t *img) {
	void *ME_vaddr;
	size_t size, fdt_size, initrd_size;
	void *fdt_vaddr, *initrd_vaddr;
	void *dest_ME_vaddr;
	int section_nr;
	uint32_t current_pgtable_paddr;
	uint32_t *pgtable_from;
	uint32_t initrd_start, initrd_end;
	int nodeoffset;
	int ret;

	/* Pick the current pgtable from the agency and copy the PTEs corresponding to the user space region. */
	current_pgtable_paddr = cpu_get_l1pgtable();
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);
	pgtable_from = (uint32_t *) __lva(current_pgtable_paddr);

	/* Get the visibility on the domain image stored in the agency user space area */
	for (section_nr = 0x0; section_nr < 0xc00; section_nr++)
		((uint32_t *) swapper_pg_dir)[section_nr] = pgtable_from[section_nr];

	flush_all();

	/* Get the pointer to the OS binary image from the ITB we got from the user space. */
	fit_image_get_data_and_size(img, fit_image_get_node(img, "kernel"), (const void **) &ME_vaddr, &size);

	/* Get the associated device tree. */
	fit_image_get_data_and_size(img, fit_image_get_node(img, "fdt"), (const void **) &fdt_vaddr, &fdt_size);

	/* Get the initrd if any. */
	ret = fit_image_get_node(img, "ramdisk");
	if (ret >= 0)
		ret = fit_image_get_data_and_size(img, ret, (const void **) &initrd_vaddr, &initrd_size);

	dest_ME_vaddr = (void *) __lva(memslot[slotID].base_paddr);

	dest_ME_vaddr += L_TEXT_OFFSET;

	/* Move the kernel binary within the domain slotID. */
	memcpy(dest_ME_vaddr, ME_vaddr, size);

	/* Put the FDT device tree close to the top of memory allocated to the domain.
	 * Since there is the initial domain stack at the top, we put the FDT one page (PAGE_SIZE) lower.
	 */
	memslot[slotID].fdt_paddr = memslot[slotID].base_paddr + memslot[slotID].size - PAGE_SIZE;

	memcpy((void *) __lva(memslot[slotID].fdt_paddr), fdt_vaddr, fdt_size);

	/* We put then the initrd (if any) right under the device tree. */

	if (ret == 0) {
		/* Expand the device tree */
		fdt_open_into((void *) __lva(memslot[slotID].fdt_paddr), (void *) __lva(memslot[slotID].fdt_paddr), fdt_size+128);

		/* find or create "/chosen" node. */
		nodeoffset = fdt_find_or_add_subnode((void *) __lva(memslot[slotID].fdt_paddr), 0, "chosen");
		BUG_ON(nodeoffset < 0);

		initrd_start = memslot[slotID].fdt_paddr - initrd_size;
		initrd_start = ALIGN_DOWN(initrd_start, PAGE_SIZE);
		initrd_end = initrd_start + initrd_size;

		ret = fdt_setprop_u32((void *) __lva(memslot[slotID].fdt_paddr), nodeoffset, "linux,initrd-start", (uint32_t) initrd_start);
		BUG_ON(ret != 0);

		ret = fdt_setprop_u32((void *) __lva(memslot[slotID].fdt_paddr), nodeoffset, "linux,initrd-end", (uint32_t) initrd_end);
		BUG_ON(ret != 0);

		memcpy((void *) __lva(initrd_start), initrd_vaddr, initrd_size);
	}
}

