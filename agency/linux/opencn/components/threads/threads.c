/********************************************************************
* Description:  threads.c
*               This file, 'threads.c', is a HAL component that
*               provides a way to create realtime threads but
*               contains no other functionality.
*
* Author: John Kasunich
* License: GPL Version 2
*
* Copyright (c) 2003 All rights reserved.
*
* Last change:
********************************************************************/
/** This file, 'threads.c', is a HAL component that provides a way to
    create realtime threads but contains no other functionality.
    It will mostly be used for testing - when EMC is run normally,
    the motion module creates all the neccessary threads.

    The module has three pairs of parameters, "name1, period1", etc.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include <opencn/opencn.h>

#ifdef CONFIG_ARM
#include <asm/neon.h>
#endif

#ifdef CONFIG_X86
#include <asm/fpu/api.h>
#endif

#include <opencn/rtapi/rtapi.h>            /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h>        /* RTAPI realtime module decls */
#include <opencn/hal/hal.h>                /* HAL public API decls */

#include <opencn/rtapi/rtapi_errno.h>
#include <opencn/rtapi/rtapi_string.h>

#include <linux/uaccess.h>

#include <opencn/uapi/threads.h>

#include <soo/uapi/soo.h>

static int comp_id;		/* component ID */

/*
 * The task period is received as us (microseconds) and NOT in nanoseconds.
 * However, the underlying layers works mainly with nanoseconds, thus the conversion.
 */
int threads_app_main(threads_connect_args_t __user *args)
{
    int retval;
    char *name1, *name2, *name3;
    long period1, period2, period3;
    int fp1, fp2, fp3;

    name1 = args->name1;
    period1 = args->period1;
    fp1 = args->fp1;

    name2 = args->name2;
    period2 = args->period2;
    fp2 = args->fp2;

    name3 = args->name3;
    period3 = args->period3;
    fp3 = args->fp3;

    /* have good config info, connect to the HAL */
    comp_id = hal_init(__core_hal_user, "threads");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "THREADS: ERROR: hal_init() failed\n");
	return -1;
    }

    /* was 'period' specified in the insmod command? */
    if ((period1 > 0) && (name1 != NULL) && (*name1 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(__core_hal_user, name1, MICROSECS(period1), fp1);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name1);
	    hal_exit(__core_hal_user, comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period1);
	}
    }
    if ((period2 > 0) && (name2 != NULL) && (*name2 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(__core_hal_user, name2, MICROSECS(period2), fp2);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name2);
	    hal_exit(__core_hal_user, comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period2);
	}
    }
    if ((period3 > 0) && (name3 != NULL) && (*name3 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(__core_hal_user, name3, MICROSECS(period3), fp3);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name3);
	    hal_exit(__core_hal_user, comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period3);
	}
    }
    hal_ready(__core_hal_user, comp_id);

    return 0;
}

void threads_app_exit(void)
{
    hal_exit(__core_hal_user, comp_id);
}


/****************/

int threads_open(struct inode *inode, struct file *file) {
	return 0;
}

int threads_release(struct inode *inode, struct file *filp) {
	return 0;
}

long threads_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	int rc = 0, major, minor;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {

		case THREADS_IOCTL_CONNECT:

			rc = threads_app_main((threads_connect_args_t *) arg);

			if (rc) {
				printk("%s: failed to initialize...\n", __func__);
				goto out;
			}

			break;
	}
out:
	return rc;
}

struct file_operations threads_fops = {
		.owner = THIS_MODULE,
		.open = threads_open,
		.release = threads_release,
		.unlocked_ioctl = threads_ioctl,
};

int threads_comp_init(void) {

	int rc;

	printk("OpenCN: threads subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(THREADS_DEV_MAJOR, THREADS_DEV_NAME, &threads_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", THREADS_DEV_MAJOR);
		return rc;
	}

	return 0;
}

late_initcall(threads_comp_init)







