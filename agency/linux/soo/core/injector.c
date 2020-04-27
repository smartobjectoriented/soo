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

#include <soo/me_bin.h>

size_t ME_size;

void *ME_buffer;

size_t current_size = 0;

int injector_receive_ME(void *ME, size_t size) {
	memcpy((uint8_t *)ME_buffer+current_size, (uint8_t *)ME, size);
	current_size += size;

	return 0;
}


void injector_prepare(uint32_t size) {
	ME_size = size;

	//ME_buffer = __vmalloc(size, GFP_HIGHUSER | __GFP_ZERO, PAGE_SHARED | PAGE_KERNEL);
	//cache_flush_all();
	ME_buffer = vmalloc(size);
	cache_flush_all();
	// ME_buffer = kmalloc(size, GFP_HIGHUSER | __GFP_ZERO);

	BUG_ON(ME_buffer == NULL);
}

/* /dev interface */

static int injector_open(struct inode *inode, struct file *file) {
	return 0;
}

static void injector_clean_ME(void) {
	vfree((void *)ME_buffer);
	ME_size = 0;
	current_size = 0;
}


static long injector_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	injector_ioctl_recv_args_t args;

	switch (cmd) {

	case INJECTOR_IOCTL_CLEAN_ME:
		injector_clean_ME();
		return 0;

	case INJECTOR_IOCTL_RETRIEVE_ME:
		/* Check if an ME is present */
		if (ME_size == 0 || (current_size != ME_size)) {
			args.ME_data = NULL;
			args.size = 0;
			return 0;
		}

		args.ME_data = ME_buffer;
		args.size = ME_size;

		if ((copy_to_user((void *) arg, &args, sizeof(injector_ioctl_recv_args_t))) != 0) {
			lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
			BUG();
		}
		return 0;
	

	default:
		lprintk("%s: DCM ioctl %d unavailable...\n", __func__, cmd);
		panic("DCM core");
	}

	return -EINVAL;
}

static struct file_operations injector_fops = {
	.owner          = THIS_MODULE,
	.open           = injector_open,
	.unlocked_ioctl = injector_ioctl,
};


/**
 * DCM initialization function.
 */
static int injector_init(void) {
	int rc;

	DBG("Injector subsys initializing ...\n");
	
	/* Registering device */
	rc = register_chrdev(INJECTOR_MAJOR, INJECTOR_DEV_NAME, &injector_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", INJECTOR_MAJOR);
		BUG();
	}

#if 0 /* Enable to simulate an ME injection at boot */  

	ME_buffer = vmalloc(ME_length);

	memcpy(ME_buffer, ME, ME_length);

	ME_size = ME_length;
	current_size = ME_length;
#endif

	return 0;
}

device_initcall(injector_init);
