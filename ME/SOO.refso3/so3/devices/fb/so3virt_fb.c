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


void *mmap(int fd, uint32_t virt_addr, uint32_t page_count);

/* Framebuffer's physical address */
static uint32_t fb_base;

struct file_operations vfb_fops = {
	.mmap = mmap
};

struct devclass vfb_cdev = {
	.class = DEV_CLASS_FB,
	.type = VFS_TYPE_DEV_FB,
	.fops = &vfb_fops,
};


/*
 * Initialisation of the PL111 CLCD Controller.
 * Linux driver: video/fbdev/amba-clcd.c
 */
int fb_init(dev_t *dev)
{
	uint32_t fb;

	/* Allocate contiguous memory for the framebuffer and get the physical address. */
	fb = get_contig_free_vpages(1024 * 768 * 4 / PAGE_SIZE);
	fb_base = virt_to_phys_pt(fb);

	*(uint32_t*)fb = 0x12345678; // test: set value of first pixel
	DBG("so3virt_fb, virt 0x%08x, phys 0x%08x\n", fb, fb_base);

	/* Register the framebuffer so it can be accessed from user space. */
	devclass_register(dev, &vfb_cdev);

	return 0;
}

void *mmap(int fd, uint32_t virt_addr, uint32_t page_count)
{
	uint32_t i, page;
	pcb_t *pcb = current()->pcb;

	for (i = 0; i < page_count; i++) {
		/* Map a process' virtual page to the physical one (here the VRAM). */
		page = fb_base + i * PAGE_SIZE;
		create_mapping(pcb->pgtable, virt_addr + (i * PAGE_SIZE), page, PAGE_SIZE, false);
		add_page_to_proc(pcb, phys_to_page(page));
	}

	return (void *) virt_addr;
}

/* Return the physical address of the framebuffer. */
uint32_t get_fb_base(void)
{
	return fb_base;
}

REGISTER_DRIVER_POSTCORE("fb,so3virt", fb_init);
