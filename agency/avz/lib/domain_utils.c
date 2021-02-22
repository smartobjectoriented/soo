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

#include <libfdt/fdt_support.h>

#include <device/fdt.h>

#include <memslot.h>
#include <sched.h>

#include <lib/image.h>

#include <soo/soo.h>
#include <soo/uapi/avz.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#define L_TEXT_OFFSET	0x8000

/**
 * We put all the guest domains in ELF format on top of memory so
 * that the domain_build will be able to elf-parse and load to their final destination.
 */
void loadAgency(void)
{
	addr_t dom_addr;
	int nodeoffset, next_node;
	uint8_t tmp[16];
	u64 base, size;
	int len, depth, ret;
	const char *propstring;

	nodeoffset = 0;
	depth = 0;
	while (nodeoffset >= 0) {
		next_node = fdt_next_node(fdt_vaddr, nodeoffset, &depth);
		ret = fdt_property_read_string(fdt_vaddr, nodeoffset, "type", &propstring);
		if ((ret != -1) && (!strcmp(propstring, "agency"))) {

#if BITS_PER_LONG == 32
			ret = fdt_property_read_u32(fdt_vaddr, nodeoffset, "load-addr", (u32 *) &dom_addr);
#else
			ret = fdt_property_read_u64(fdt_vaddr, nodeoffset, "load-addr", (u64 *) &dom_addr);
#endif

			if (ret == -1) {
				lprintk("!! Missing load-addr in the agency node !!\n");
				BUG();
			} else
			  break;
		}
		nodeoffset = next_node;
	}

	if (nodeoffset < 0) {
		lprintk("!! Unable to find a node with type agency in the FIT image... !!\n");
		BUG();
	}

	/* Set the memslot base address to a section boundary */
	memslot[MEMSLOT_AGENCY].base_paddr = (dom_addr & ~(SZ_1M - 1));
	memslot[MEMSLOT_AGENCY].fdt_paddr = (addr_t) __fdt_addr;
	memslot[MEMSLOT_AGENCY].size = fdt_getprop_u32_default(fdt_vaddr, "/agency", "domain-size", 0);
	
	/* Fixup the agency device tree */

	/* find or create "/memory" node. */
	nodeoffset = fdt_find_or_add_subnode(fdt_vaddr, 0, "memory");
	BUG_ON(nodeoffset < 0);

	fdt_setprop(fdt_vaddr, nodeoffset, "device_type", "memory", sizeof("memory"));

	base = (u64) memslot[MEMSLOT_AGENCY].base_paddr;
	size = (u64) memslot[MEMSLOT_AGENCY].size;

	len = fdt_pack_reg(fdt_vaddr, tmp, &base, &size);

	fdt_setprop(fdt_vaddr, nodeoffset, "reg", tmp, len);
}


/*
 * The concatened image must be out of domains because of elf parser
 *
 * <img> represents the original binary image as injected in the user application
 * <target_dom> represents the target memory area
 */
void loadME(unsigned int slotID, uint8_t *img, addrspace_t *current_addrspace) {
	void *ME_vaddr;
	size_t size, fdt_size, initrd_size;
	void *fdt_vaddr, *initrd_vaddr;
	void *dest_ME_vaddr;
	int section_nr;
	uint32_t *pgtable_from;
	uint32_t initrd_start, initrd_end;
	int nodeoffset, next_node, depth = 0;
	int ret;
	const char *propstring;

	pgtable_from = (uint32_t *) __lva(current_addrspace->pgtable_paddr);

#warning to be revisited...
#if 0
	/* Get the visibility on the domain image stored in the agency user space area */
	for (section_nr = 0x0; section_nr < 0xc00; section_nr++)
		__sys_l1pgtable[section_nr] = pgtable_from[section_nr];

	flush_dcache_all();
#endif
	/* Look for a node of ME type in the fit image */
	nodeoffset = 0;
	depth = 0;
	while (nodeoffset >= 0) {
		next_node = fdt_next_node((void *) img, nodeoffset, &depth);
		ret = fdt_property_read_string(img, nodeoffset, "type", &propstring);

		if ((ret != -1) && !strcmp(propstring, "ME")) {

			/* Get the pointer to the OS binary image from the ITB we got from the user space. */
			ret = fit_image_get_data_and_size(img, nodeoffset, (const void **) &ME_vaddr, &size);
			if (ret) {
				lprintk("!! The properties in the ME node does not look good !!\n");
				BUG();
			} else
				break;
		}
		nodeoffset = next_node;
	}

	if (nodeoffset < 0) {
		lprintk("!! Unable to find a node with type ME in the FIT image... !!\n");
		BUG();
	}

	/* Look for a node of flat_dt type in the fit image */
	nodeoffset = 0;
	depth = 0;
	while (nodeoffset >= 0) {
		next_node = fdt_next_node((void *) img, nodeoffset, &depth);
		ret = fdt_property_read_string(img, nodeoffset, "type", &propstring);
		if ((ret != -1) && !strcmp(propstring, "flat_dt")) {

			/* Get the associated device tree. */
			ret = fit_image_get_data_and_size(img, nodeoffset, (const void **) &fdt_vaddr, &fdt_size);
			if (ret) {
				lprintk("!! The properties in the device tree node does not look good !!\n");
				BUG();
			} else
				break;
		}
		nodeoffset = next_node;
	}

	if (nodeoffset < 0) {
		lprintk("!! Unable to find a node with type flat_dt in the FIT image... !!\n");
		BUG();
	}

	/* Look for a possible node of ramdisk type in the fit image */
	nodeoffset = 0;
	depth = 0;
	while (nodeoffset >= 0) {
		next_node = fdt_next_node(img, nodeoffset, &depth);
		ret = fdt_property_read_string(img, nodeoffset, "type", &propstring);
		if ((ret != -1) && !strcmp(propstring, "ramdisk")) {

			/* Get the associated device tree. */
			ret = fit_image_get_data_and_size(img, nodeoffset, (const void **) &initrd_vaddr, &initrd_size);
			if (ret) {
				lprintk("!! The properties in the ramdisk node does not look good !!\n");
				BUG();
			} else
				break;
		}
		nodeoffset = next_node;
	}

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

