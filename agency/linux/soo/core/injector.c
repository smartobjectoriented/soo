/*
 * Copyright (C) 2017 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <soo/core/device_access.h>
#include <soo/core/migmgr.h>

#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/injector.h>

/* Buffer in which the ME will be received. It is dynamically allocated
in injector_prepare */
uint8_t *ME_buffer;
/* full size of the me we are receiving */
size_t ME_size;
/* Current size received */
size_t current_size = 0;

/**
 * Initiate the injection of a ME.
 *
 * @param buffer
 * @return slotID or -1 if no slotID available.
 */
int inject_ME(void *buffer, size_t size) {
	int slotID;

	DBG("Original contents at address: 0x%08x\n with size %d bytes", (unsigned int) buffer, size);

	soo_hypercall(AVZ_INJECT_ME, buffer, NULL, &slotID, &size);

	return slotID;
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
