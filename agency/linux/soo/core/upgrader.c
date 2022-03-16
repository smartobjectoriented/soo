/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#include <soo/vbus.h>

#include <soo/uapi/console.h>

#include <soo/core/upgrader.h>

/* For the upgrade */
addr_t upgrade_buffer_pfn = 0;
uint32_t upgrade_buffer_size = 0;
unsigned int upgrade_ME_slotID = 5;

/* Get the pfn of the upgrade image */
uint32_t upg_get_pfn(void) {
    return upgrade_buffer_pfn;
}

uint32_t upg_get_size(void) {
    return upgrade_buffer_size;
}

unsigned int upg_get_ME_slotID(void) {
    return upgrade_ME_slotID;
}

void upg_store_addr(addr_t buffer_pfn, uint32_t buffer_size) {

    upgrade_buffer_pfn = buffer_pfn;
    upgrade_buffer_size = buffer_size;
}

void upg_store(uint32_t buffer_pfn, uint32_t buffer_size, unsigned int ME_slotID) {

    printk("[soo:core:upgrader] Storing upgrade image: pfn %u, size %u, slot %d\n", buffer_pfn, buffer_size, ME_slotID);

    upgrade_buffer_pfn = buffer_pfn;
    upgrade_buffer_size = buffer_size;
    upgrade_ME_slotID = ME_slotID;
}

void get_upgrade_image(uint32_t *size, uint32_t *slotID) {

	*size = upg_get_size();
	*slotID = upg_get_ME_slotID();
}

void store_versions(upgrade_versions_args_t *versions) {

	vbus_printf(VBT_NIL, "/soo", "itb-version", "%u", versions->itb);
	vbus_printf(VBT_NIL, "/soo", "uboot-version", "%u", versions->uboot);
	vbus_printf(VBT_NIL, "/soo", "rootfs-version", "%u", versions->rootfs);
}

/**
 * This is the SOO Core mmap implementation. It is used to map the upgrade
 * image which is in the ME.
 */
int agency_upgrade_mmap(struct file *filp, struct vm_area_struct *vma) {
	unsigned long start, size;

	start = upg_get_pfn();
	size = vma->vm_end - vma->vm_start;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot = phys_mem_access_prot(filp, vma->vm_pgoff, size, vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, start, size, vma->vm_page_prot)) {
		lprintk("%s: remap_pfn failed.\n", __func__);
		BUG();
	}

	return 0;
}




