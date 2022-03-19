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

#if 1
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

size_t ME_size;

uint8_t *ME_buffer;

size_t current_size = 0;

void *tmp_buf;
size_t tmp_size;

bool full = false;

DECLARE_WAIT_QUEUE_HEAD(wq_prod);
DECLARE_WAIT_QUEUE_HEAD(wq_cons);
DECLARE_COMPLETION(me_ready_compl);

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

#if 0
	wait_event_interruptible(wq_prod, !full);
#endif
	memcpy(ME_buffer+current_size, ME, size);

	current_size += size;

	/* We received the full ME */ 
	if (current_size == ME_size) {
		int slotID = -1;

		slotID = inject_ME(ME_buffer, ME_size);
		if (slotID != -1) {
			soo_log("[soo:injector] Finalizing migration in slot %d\n", slotID);
			finalize_migration(slotID);
		}

		injector_clean_ME();
	}
#if 0
	tmp_buf = ME;
	tmp_size = size;

	full = true;

	wake_up_interruptible(&wq_cons);
#endif	
}

void *injector_get_tmp_buf(void) {
	return tmp_buf;
}

size_t injector_get_tmp_size(void) {
	return tmp_size;
}

bool injector_is_full(void) {
	return full;
}

void injector_set_full(bool _full) {
	full = _full;
}

void *injector_get_ME_buffer(void) {
	return ME_buffer;
}

size_t injector_get_ME_size(void) {
	return ME_size;
}

void injector_prepare(uint32_t size) {
	current_size = 0;
	ME_size = size;
#if 0	
	complete(&me_ready_compl);
#endif	
	ME_buffer = vzalloc(size);
}


void injector_clean_ME(void) {
	vfree((void *)ME_buffer);
	ME_size = 0;
	current_size = 0;
}


uint32_t injector_retrieve_ME(void) {

	/* Wait until a ME has started to be received. Will be woke up by 
	   a call to injector_prepare from the vuihandler */
	wait_for_completion(&me_ready_compl);

	return ME_size;
}

/**
 * Read callback function to interact with the user space.
 * The user space reads chunks of ME.
 *
 * @param fp
 * @param buff
 * @param length
 * @param ppos
 * @return
 */
ssize_t agency_read(struct file *fp, char *buff, size_t length, loff_t *ppos) {
	int maxbytes;
	int bytes_to_read;
	int bytes_read;
	void *ME;

	/* Wait for the Injector to produce data */
	wait_event_interruptible(wq_cons, injector_is_full() == true);

	ME = injector_get_tmp_buf();
	maxbytes = injector_get_tmp_size();

	if (maxbytes > length)
		bytes_to_read = length;
	else
		bytes_to_read = maxbytes;

	bytes_read = copy_to_user(buff, ME, bytes_to_read);

	/* Notify the Injector we read the buffer */
	injector_set_full(false);
	wake_up_interruptible(&wq_prod);

#warning temp trick...
	vfree(ME-1);

    	return bytes_to_read;
}

