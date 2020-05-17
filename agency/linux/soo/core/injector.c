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

#include <soo/uapi/injector.h>

#include <soo/core/device_access.h>

#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>

size_t ME_size;

void *ME_buffer;

size_t current_size = 0;

void *tmp_buf;
size_t tmp_size;



DEFINE_MUTEX(injection_lock);


/**
 * injector_receive_ME() - Receive the ME into the injector buffer
 *
 * @ME: pointer to the ME chunk received from the vUIHandler 
 * @size: Size of the ME chunk
 * @return 0 
 */
int injector_receive_ME(void *ME, size_t size) {
	memcpy((uint8_t *)ME_buffer+current_size, (uint8_t *)ME, size);
	current_size += size;
	tmp_buf = ME;
	tmp_size = size;

	printk("Before lock\n");
	mutex_lock(&injection_lock);
	printk("After lock\n");

	return 0;
}

void *injector_get_tmp_buf(void) {
	return tmp_buf;
}
size_t injector_get_tmp_size(void) {
	return tmp_size;
}

void *injector_get_ME_buffer(void) {
	return ME_buffer;
}

size_t injector_get_ME_size(void) {
	return ME_size;
}


void injector_prepare(uint32_t size) {
	ME_size = size;

	ME_buffer = vmalloc(size);

	BUG_ON(ME_buffer == NULL);
}


void injector_clean_ME(void) {
	vfree((void *)ME_buffer);
	ME_size = 0;
	current_size = 0;
}


void injector_retrieve_ME(unsigned long arg) {
	injector_ioctl_recv_args_t args;

	/* Check if an ME is present */
	if (ME_size == 0 || (current_size != ME_size)) {
		args.ME_data = NULL;
		args.size = 0;
		return;
	}

	args.ME_data = ME_buffer;
	args.size = ME_size;

	if ((copy_to_user((void *) arg, &args, sizeof(injector_ioctl_recv_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
		BUG();
	}
}


/**
 * Injector initialization function.
 */
int injector_init(struct mutex lock) {

	DBG("Injector subsys initializing ...\n");


	injection_lock = lock;

#if 0 /* Enable to simulate an ME injection at boot */  
	printk("ME INJECTION SIMULATED!\n");
	ME_buffer = vmalloc(ME_length);

	memcpy(ME_buffer, ME, ME_length);

	ME_size = ME_length;
	current_size = ME_length;
#endif

	return 0;
}
