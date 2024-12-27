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

        return args.u.avz_inject_me_args.slotID;
}


/**
 * injector_receive_ME() - Receive the ME into the injector buffer
 *
 * @ME: pointer to the ME chunk received from the vUIHandler 
 * @size: Size of the ME chunk
 */
void injector_receive_ME(void *ME, size_t size) {

	memcpy(ME_buffer+current_size, ME, size);
	current_size += size;

	/* We received the full ME */ 
	if (current_size == ME_size) {
		int slotID = -1;

		/* Inject it, and if successful, finalize the migration */
		slotID = inject_ME(ME_buffer, ME_size);
		if (slotID != -1) {
			soo_log("[soo:injector] Finalizing migration in slot %d\n", slotID);
			finalize_migration(slotID);
		}
		/* Free the Injector internal buffer */
		injector_clean_ME();
	}	
}

/**
 * Allocate the ME and handle the sizes for the upcoming injection.
 * It is called once at the begining of the reception.
 * 
 * @size: Size of the ME ITB which will be injected.
 */
void injector_prepare(uint32_t size) {
	current_size = 0;
	ME_size = size;

	/* The buffer is allocated here and freed once the ME is completely received
	and injected in the `injector_receive_ME` function */
	ME_buffer = vzalloc(size);
	if (ME_buffer == NULL) {
		lprintk("[Injector][%s]: Cannot allocate the ME buffer!\n", __func__);
		BUG();
	}
}


void injector_clean_ME(void) {
	vfree((void *)ME_buffer);
	ME_size = 0;
	current_size = 0;
}
