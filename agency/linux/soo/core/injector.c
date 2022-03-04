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

#include <soo/uapi/injector.h>

#include <soo/core/device_access.h>

#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/injector.h>

size_t ME_size;

void *ME_buffer;

size_t current_size = 0;

void *tmp_buf;
size_t tmp_size;

bool full = false;

DECLARE_WAIT_QUEUE_HEAD(wq_prod);
DECLARE_WAIT_QUEUE_HEAD(wq_cons);
DECLARE_COMPLETION(me_ready_compl);

/**
 * Initiate the injection of a ME.
 */
int ioctl_inject_ME(unsigned long arg) {
	agency_tx_args_t args;

	if (copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	DBG("Original contents at address: 0x%08x\n", (unsigned int) args.buffer);

	/* We use paddr1 to pass virtual address of the crc32 */
	if (soo_hypercall(AVZ_INJECT_ME, args.buffer, NULL, &args.ME_slotID, NULL) < 0) {
		lprintk("Agency: %s:%d Failed to finalize migration.\n", __func__, __LINE__);
		BUG();
	}

	if ((copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}


/**
 * injector_receive_ME() - Receive the ME into the injector buffer
 *
 * @ME: pointer to the ME chunk received from the vUIHandler 
 * @size: Size of the ME chunk
 * @return 0 
 */
int injector_receive_ME(void *ME, size_t size) {

#if 0
	printk("%d\n", size);
#endif	
	wait_event_interruptible(wq_prod, !full);

	current_size += size;

	tmp_buf = ME;
	tmp_size = size;

	full = true;

	wake_up_interruptible(&wq_cons);

	return 0;
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
	ME_size = size;
	complete(&me_ready_compl);
}


void injector_clean_ME(void) {
	vfree((void *)ME_buffer);
	ME_size = 0;
	current_size = 0;
}


void injector_retrieve_ME(unsigned long arg) {
	injector_ioctl_recv_args_t args;


	/* Wait until a ME has started to be received. Will be woke up by 
	   a call to injector_prepare from the vuihandler */
	wait_for_completion(&me_ready_compl);

	args.ME_data = ME_buffer;
	args.size = ME_size;

	if ((copy_to_user((void *) arg, &args, sizeof(injector_ioctl_recv_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
		BUG();
	}
}

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

    	return bytes_to_read;
}

