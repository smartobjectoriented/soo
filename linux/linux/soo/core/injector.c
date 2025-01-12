/*
 * Copyright (C) 2017-2024 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/completion.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/dma-direct.h>

#include <soo/core/device_access.h>
#include <soo/core/migmgr.h>
#include <soo/soo.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/injector.h>
#include <soo/uapi/avz.h>

/* Buffer in which the ME will be received. It is dynamically allocated
in injector_prepare */
uint8_t *ME_buffer;

/* Full size of the me we are receiving */
size_t ME_size;

/* Current size received */
size_t current_size = 0;

#define CMA_MALLOC_DEVICE_FILENAME "cma_malloc"

static struct file_operations cma_malloc_fileops = {
    .owner          =   THIS_MODULE,
};

static struct miscdevice cma_malloc_miscdevice = {
    .minor           =   MISC_DYNAMIC_MINOR,
    .name            =   CMA_MALLOC_DEVICE_FILENAME,
    .fops            =   &cma_malloc_fileops,
    .mode            =   S_IRUGO | S_IWUGO,
};

/**
 * Initiate the injection of a ME.
 * 
 * The ME is first copied within the kernel heap so that
 * we get rid of user space paging and have a contiguous memory allocation.
 * 
 * @param buffer
 * @return slotID or -1 if no slotID available.
 */
int inject_ME(void *buffer, size_t size) {
        void *me = NULL;
	dma_addr_t dma_handle;
        struct device *dev;
	int ret;
        avz_hyp_t args;

        DBG("Original contents at address: 0x%08x\n with size %d bytes\n", (unsigned long) buffer, size);

    	ret = misc_register(&cma_malloc_miscdevice);
        BUG_ON(ret);

        dev = cma_malloc_miscdevice.this_device;
        dev->coherent_dma_mask = DMA_BIT_MASK(32);
        dev->dma_mask = &dev->coherent_dma_mask;

        /* Allocate a contiguous memory region to host the ME */
        me = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
        BUG_ON(!me);
     
        memcpy(me, buffer, size);

        /* Since the ME buffer is in the CMA zone and allocated via the
         * dma_alloc_cohenrent() function, we cannot use virt_to_phys(). So, we
         * take the CPU phys addr stored in dma_handle, and we force to a
         * (wrong) virtual address to make sure the soo_hypercall() function
         * gets the right physical address; indeeed, it uses virt_to_phys() there.
         */

        args.cmd = AVZ_INJECT_ME;

        args.u.avz_inject_me_args.itb_paddr = (void *) dma_handle;
        avz_hypercall(&args);
                
        dma_free_coherent(dev, size, me, dma_handle);
	
	misc_deregister(&cma_malloc_miscdevice);

        return args.u.avz_inject_me_args.slotID;
}

/**
 * Read a ME snapshot for migration or saving.
 * The ME is read and stored in a vmalloc'd memory area.
 *
 * @param slotID
 * @param buffer pointer to the ME buffer
 */
void read_snapshot(uint32_t slotID, void *buffer, uint32_t *size) {
	ME_desc_t ME_desc;
        avz_hyp_t args;
        struct device *dev;
	void *me = NULL;
        int ret;
	dma_addr_t dma_handle;

        /* Ask the size only */
        if (*size == 0) {
                args.cmd = AVZ_ME_READ_SNAPSHOT;
                args.u.avz_snapshot_args.slotID = slotID;
                args.u.avz_snapshot_args.size = 0;

                avz_hypercall(&args);
                *size = args.u.avz_snapshot_args.size;
                
                return;
        }

	ret = misc_register(&cma_malloc_miscdevice);

        dev = cma_malloc_miscdevice.this_device;
        dev->coherent_dma_mask = DMA_BIT_MASK(32);
        dev->dma_mask = &dev->coherent_dma_mask;

       	/*
	 * Prepare a buffer to store the ME and additional header information like migration structure.
	 */

        me = dma_alloc_coherent(dev, *size, &dma_handle, GFP_KERNEL);
        BUG_ON(!me);

        args.cmd = AVZ_ME_READ_SNAPSHOT;
        args.u.avz_snapshot_args.slotID = slotID;
        args.u.avz_snapshot_args.snapshot_paddr = (void *) dma_handle;
        
        avz_hypercall(&args);

        /* Copy the snapshot to the user buffer */
        ret = copy_to_user(buffer, me, *size);
        BUG_ON(ret);
}

/**
 * Write a ME snapshot provided a ME_info_transfer_t descriptor.
 *
 * @param slotID
 * @param buffer  Adresse of a buffer of ME_info_transfert_t
 */
void write_snapshot(uint32_t slotID, void *buffer) {
	ME_desc_t ME_desc;
	void *target;
        avz_hyp_t args;

#if 0
        /* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(slotID, &ME_desc);

	/* Beginning of the ME_buffer */
	ME_info_transfer = (ME_info_transfer_t *) buffer;

	/* Retrieve the info related to the migration structure */
	memcpy(__buffer, buffer + sizeof(ME_info_transfer_t), ME_info_transfer->size_mig_structure);

        args.cmd = AVZ_MIG_WRITE_MIGRATION_STRUCT;
        args.u.avz_migstruct_write_args.migstruct_paddr = (void *) virt_to_phys(__buffer);
        
	avz_hypercall(&args);

        /* We got the pfn of the local destination for this ME, therefore... */

        target = paging_remap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(target == NULL);

	/* Finally, perform the copy */
	memcpy(target, (void *) (buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure), ME_desc.size);

	/* Relase the map used to copy the ME to its final location */
	iounmap(target);
#endif
}

