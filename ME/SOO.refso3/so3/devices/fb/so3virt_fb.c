/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <heap.h>
#include <vfs.h>
#include <common.h>
#include <process.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <device/driver.h>
#include <device/fb/so3virt_fb.h>

#define FB_SIZE (1024 * 768 * 4)

void *mmap(int fd, uint32_t virt_addr, uint32_t page_count);

struct file_operations vfb_fops = {
	.mmap = mmap
};

struct devclass vfb_cdev = {
	.class = DEV_CLASS_FB,
	.type = VFS_TYPE_DEV_FB,
	.fops = &vfb_fops,
};

/* Framebuffer's physical address */
static uint32_t fb_base;

/*
 * Initialisation of the PL111 CLCD Controller.
 * Linux driver: video/fbdev/amba-clcd.c
 */
int fb_init(dev_t *dev)
{
	/*
	 * Allocate contiguous memory for the framebuffer and get the physical address.
	 * The pages will be never released. They do not belong to any process.
	 */
	fb_base = get_contig_free_pages(FB_SIZE / PAGE_SIZE);
	DBG("so3virt_fb: allocated %d [phys 0x%08x]\n", FB_SIZE, fb_base);

	/* Register the framebuffer so it can be accessed from user space. */
	devclass_register(dev, &vfb_cdev);

	return 0;
}

void *mmap(int fd, uint32_t virt_addr, uint32_t page_count)
{
	uint32_t i, page_phys_base;
	pcb_t *pcb = current()->pcb;

	for (i = 0; i < page_count; i++) {
		/* Map the process' pages to physical ones. */
		create_mapping(pcb->pgtable, virt_addr + (i * PAGE_SIZE), fb_base + i * PAGE_SIZE, PAGE_SIZE, false);
	}

	return (void *) virt_addr;
}

/* Return the physical address of the framebuffer. */
uint32_t get_fb_base(void)
{
	return fb_base;
}

REGISTER_DRIVER_POSTCORE("fb,so3virt", fb_init);
