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

/*
 * Driver for the PL111 CLCD controller.
 *
 * Documentation:
 *   http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0293c/index.html
 */

#if 1
#define DEBUG
#endif

#include <vfs.h>
#include <common.h>
#include <process.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <device/driver.h>
#include <soo/dev/vfb.h>


#define LCDUPBASE 0x18000000 /* VRAM */


void *mmap(int fd, uint32_t virt_addr, uint32_t page_count);

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
	//pcb_t *pcb = current()->pcb;
	//uint32_t virt_addr = pcb->stack_top - (pcb->page_count + 1) * PAGE_SIZE;
	//create_mapping(pcb->pgtable, virt_addr, LCDUPBASE, PAGE_SIZE, false);
	//*(uint32_t *)virt_addr = 0xffffffff;

	//write_vbstore();
	DBG("so3virt fb ok\n");

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
		page = LCDUPBASE + i * PAGE_SIZE;
		create_mapping(pcb->pgtable, virt_addr + (i * PAGE_SIZE), page, PAGE_SIZE, false);
		add_page_to_proc(pcb, phys_to_page(page));
	}

	return (void *) virt_addr;
}

REGISTER_DRIVER_POSTCORE("fb,so3virt", fb_init);