/*
 * Copyright (C) 2017 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
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

/* ME total size set before receiving it */ 
size_t ME_size;
/* Track the current size of the ME (sum of the chunks size) */
size_t current_size = 0;
/* Buffer and size which correspond to the current chunk being received */
void *ME_buf;
size_t tmp_size;
/* Indicate if data have been received */
bool full = false;

/* Wait queues for synchronization with the SOO Core. The SOO Core initialize them
   and we set them in the init */
static wait_queue_head_t *wq_cons;
static wait_queue_head_t *wq_prod;
/* Completion used to stop  */
static struct completion *bt_done;

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
 * injector_receive_ME() - Receive chunks of ME comming from the vuiHandler.
 *
 * Once a chunk is received, signal the SOO Core that it can be read.
 * Synchronization between producer (Injector) and consumer (SOO Core) 
 * is done using wait queues.
 *
 * @ME: pointer to the ME chunk received from the vuiHandler 
 * @size: Size of the ME chunk
 * @return 0 
 */
void injector_receive_ME(void *ME, size_t size) {
	/* Wait for the consumer to consume the buffer */
	//wait_event_interruptible(*wq_prod, full == false);

	current_size += size;
	/* If we fully retrieved the ME, end the BT session */
	if (current_size == ME_size)
		end_bt_session();

	/* These are the data (ME chunk) which will be read by the consumer */
	ME_buf = ME;
	tmp_size = size;

	/* Signal to the consumer that the buffer is full and wake it ip */
	full = true;
	wake_up_interruptible(wq_cons);
}

void *injector_get_ME(void) {
	return ME_buf;
}

size_t injector_get_ME_size(void) {
	return tmp_size;
}

bool injector_is_full(void) {
	return full;
}

void injector_set_full(bool _full) {
	full = _full;
}

/* Set the total ME size before receiving it */
void injector_prepare(uint32_t size) {
	ME_size = size;
}


/* Reset the sizes */
void injector_clean_ME(void) {
	ME_size = 0;
	current_size = 0;
	/* The -1 commes from the fact that we pass the received buffer offseted by one
	to the Injector to skip the vuiHandler type. */
	vfree((void *) (ME_buf-1));
}

/* Implementation of an ioctl to retrieve the ME size if a ME is being received */
void injector_retrieve_ME(unsigned long arg) {
	injector_ioctl_recv_args_t args;

	args.size = ME_size;

	if ((copy_to_user((void *) arg, &args, sizeof(injector_ioctl_recv_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
		BUG();
	}
}

/**
 * End the BT session. It complete the completion on which the Core is waiting
 * in the AGENCY_IOCTL_WAIT_BT_SESSION_DONE ioctl.
 * Once it is completed, the core 
 */
void end_bt_session(void) {
	complete(bt_done);
}

/**
 * Injector initialization function.
 * It receives the wait queues and the BT completion from the SOO core, which initialize them .
 */
int injector_init(wait_queue_head_t *_wq_prod, wait_queue_head_t *_wq_cons, struct completion *_bt_done) {
	DBG("Injector subsys initializing ...\n");
	wq_cons = _wq_cons;
	wq_prod = _wq_prod;
	bt_done = _bt_done;

	return 0;
}
