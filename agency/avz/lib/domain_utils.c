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

#include <lib/image.h>

#include <soo/uapi/avz.h>

#include <avz/init.h>
#include <avz/kernel.h>
#include <avz/sched.h>

#include <asm/memslot.h>
#include <asm/mach/map.h>

#include <soo/soo.h>

#define L_TEXT_OFFSET	0x8000

extern unsigned int fdt_paddr __initdata; /* defined in kernel/setup.c */

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
void loadME(unsigned int slotID, uint8_t *img, dtb_feat_t *dtb_feat) {
	u32 realtime;
	void *ME_vaddr;
	size_t size, fdt_size;
	void *fdt_vaddr;
	void *dest_ME_vaddr;
	int section_nr;
	uint32_t current_pgtable_paddr;
	uint32_t *pgtable_from;

	/* Pick the current pgtable from the agency and copy the PTEs corresponding to the user space region. */
	current_pgtable_paddr = cpu_get_pgd_phys();
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);
	pgtable_from = (uint32_t *) __lva(current_pgtable_paddr);

	/* Get the visibility on the domain image stored in the agency user space area */
	for (section_nr = 0x0; section_nr < 0xc00; section_nr++)
		swapper_pg_dir[section_nr].l2 = (intpte_t) pgtable_from[section_nr];

	flush_all();

	/* Get the pointer to the OS binary image from the ITB we got from the user space. */
	fit_image_get_data_and_size(img, fit_image_get_node(img, "kernel"), (const void **) &ME_vaddr, &size);

	/* Get the associated device tree. */
	fit_image_get_data_and_size(img, fit_image_get_node(img, "fdt"), (const void **) &fdt_vaddr, &fdt_size);

	dest_ME_vaddr = (void *) __lva(memslot[slotID].base_paddr);

	dest_ME_vaddr += L_TEXT_OFFSET;

	/* Move the kernel binary within the domain slotID. */
	memcpy(dest_ME_vaddr, ME_vaddr, size);

	/* Put the FDT device tree close to the top of memory allocated to the domain.
	 * Since there is the initial domain stack at the top, we put the FDT one page (PAGE_SIZE) lower.
	 */
	memslot[slotID].fdt_paddr = memslot[slotID].base_paddr + memslot[slotID].size - PAGE_SIZE;

	memcpy((void *) __lva(memslot[slotID].fdt_paddr), fdt_vaddr, fdt_size);

	realtime = fdt_getprop_u32_default((const void *) __lva(memslot[slotID].fdt_paddr), "/ME/features", "realtime", 0);

	dtb_feat->realtime = ((realtime == 1) ? true : false);

}

