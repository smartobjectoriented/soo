/********************************************************************
 * Description:  sampler.c
*               A HAL component that can be used to capture data
*               from HAL pins at a specific realtime sample rate,
*		and allows the data to be written to stdout.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*
* Copyright (c) 2006 All rights reserved.
*
 ********************************************************************/
/** This file, 'sampler.c', is the realtime part of a HAL component
 that allows numbers stored in a file to be "streamed" onto HAL
 pins at a uniform realtime sample rate.  When the realtime module
 is loaded, it creates a fifo in shared memory.  Then, the user
 space program 'hal_sampler' is invoked.  'hal_sampler' takes
 input from stdin and writes it to the fifo, and this component
 transfers the data from the fifo to HAL pins.

 */

/** Copyright (C) 2006 John Kasunich
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

/* Notes:
 * sampler.N.cur-depth, sampler.N.empty and sampler.N.underruns are
 * updated even if sampler.N.enabled is set to false.
 *
 * clock and clock_mode pins are provided to enable clocking.
 * The clock input pin actions are controlled by the clock_mode pin value:
 *   0: freerun at every loop (default)
 *   1: clock by falling edge
 *   2: clock by rising edge
 *   3: clock by any edge
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include <opencn/rtapi/rtapi.h>            /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h>        /* RTAPI realtime module decls */
#include <opencn/hal/hal.h>                /* HAL public API decls */

#include <opencn/components/streamer.h>

#include <opencn/hal/hal.h>
#include <opencn/strtox.h>

#include <opencn/ctypes/strings.h>

#include <opencn/rtapi/rtapi_errno.h>
#include <opencn/rtapi/rtapi_string.h>

#include <opencn/uapi/hal.h>
#include <opencn/uapi/sampler.h>

/***********************************************************************
 *                STRUCTURES AND GLOBAL VARIABLES                       *
 ************************************************************************/

#define	BUF_SIZE	120

/* this structure contains the HAL shared memory data for one sampler */

typedef struct {
    hal_stream_t fifo;		/* pointer to user/RT fifo */
    hal_s32_t *curr_depth;	/* pin: current fifo depth */
    hal_bit_t *full;		/* pin: overrun flag */
    hal_bit_t *enable;		/* pin: enable sampling */
    hal_bit_t *new_file;    /* pin: open a new file, using the current datetime for the filename */
    hal_s32_t *overruns;	/* pin: number of overruns */
    hal_s32_t *sample_num;	/* pin: sample ID / timestamp */
    int num_pins;
    pin_data_t pins[HAL_STREAM_MAX_PINS];
} sampler_t;

/* other globals */
static int comp_id; /* component ID */
static int nsamplers;
static sampler_t *samplers;

/***********************************************************************
 *                  LOCAL FUNCTION DECLARATIONS                         *
 ************************************************************************/

static int init_sampler(int num, sampler_t *tmp_fifo);
static void sample(void *arg, long period);

/***********************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int sampler_app_main(int n, sampler_connect_args_t *args) {
	int retval;

	comp_id = hal_init(__core_hal_user, "sampler");
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: hal_init() failed\n");
		return -EINVAL;
	}

	samplers = hal_malloc(__core_hal_user, MAX_SAMPLERS * sizeof(sampler_t));

	/* validate config info */

	retval = hal_stream_create(&samplers[n].fifo, comp_id, SAMPLER_SHMEM_KEY + n, args->depth, args->cfg);
	if (retval < 0) {
		goto fail;
	}

	retval = init_sampler(n, &samplers[n]);

	hal_ready(__core_hal_user, comp_id);

	return 0;

fail:
	for (n = 0; n < nsamplers; n++)
		hal_stream_destroy(&samplers[n].fifo);

	hal_exit(__core_hal_user, comp_id);

	return retval;
}

void sampler_app_exit(void) {
	int i;

	for (i = 0; i < nsamplers; i++)
		hal_stream_destroy(&samplers[i].fifo);

	hal_exit(__core_hal_user, comp_id);
}

/***********************************************************************
 *            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
 ************************************************************************/

static void sample(void *arg, long period)
{
	sampler_t *samp;
	pin_data_t *pptr;
	int n;
	union hal_stream_data data[HAL_STREAM_MAX_PINS], *dptr;
	int num_pins;

	/* point at sampler struct in HAL shmem */
	samp = arg;

	/* are we enabled? */
	if (!*(samp->enable) ) {
		*(samp->curr_depth) = hal_stream_depth(&samp->fifo);
		*(samp->full) = !hal_stream_writable(&samp->fifo);
		return;
	}

	/* point at pins in hal shmem */
	pptr = samp->pins;
	dptr = data;

	/* copy data from HAL pins to fifo */
	num_pins = hal_stream_element_count(&samp->fifo);

	for (n = 0; n < num_pins; n++) {
		switch (hal_stream_element_type(&samp->fifo, n)) {
			case HAL_FLOAT:
				dptr->f = *(pptr->hfloat);
				break;

			case HAL_BIT:
				if ( *(pptr->hbit) ) {
					dptr->b = 1;
				} else {
					dptr->b = 0;
				}
				break;

			case HAL_U32:
				dptr->u = *(pptr->hu32);
				break;

			case HAL_S32:
				dptr->s = *(pptr->hs32);
				break;

			default:
				break;
		}
		dptr++;
		pptr++;
	}

	if (hal_stream_write(&samp->fifo, data) < 0) {
		/* fifo is full, data is lost */
		/* log the overrun */
		(*samp->overruns)++;
		*(samp->full) = 1;
		*(samp->curr_depth) = hal_stream_maxdepth(&samp->fifo);
	} else {
		*(samp->full) = 0;
		*(samp->curr_depth) = hal_stream_depth(&samp->fifo);
	}
}

static int init_sampler(int num, sampler_t *samp) {
	int retval, usefp, n;
	pin_data_t *pptr;
	char buf[HAL_NAME_LEN + 1];

	/* export "standard" pins and params */
	retval = hal_pin_bit_newf(__core_hal_user, HAL_OUT, &(samp->full), comp_id, "sampler.%d.full", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'full' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_bit_newf(__core_hal_user, HAL_IN, &(samp->enable), comp_id, "sampler.%d.enable", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'enable' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_bit_newf(__core_hal_user, HAL_IN, &(samp->new_file), comp_id, "sampler.%d.new-file", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'new-file' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_s32_newf(__core_hal_user, HAL_OUT, &(samp->curr_depth), comp_id, "sampler.%d.curr-depth", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'curr_depth' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_s32_newf(__core_hal_user, HAL_IO, &(samp->overruns), comp_id, "sampler.%d.overruns", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'overruns' parameter export failed\n");
		return -EIO;
	}
	retval = hal_pin_s32_newf(__core_hal_user, HAL_IO, &(samp->sample_num), comp_id, "sampler.%d.sample-num", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'sample-num' parameter export failed\n");
		return -EIO;
	}
	/* init the standard pins and params */
	*(samp->full) = 0;
	*(samp->enable) = 1;
	*(samp->curr_depth) = 0;
	*(samp->overruns) = 0;
	*(samp->new_file) = 0;
	*(samp->sample_num) = 0;
	pptr = samp->pins;
	usefp = 0;
	/* export user specified pins (the ones that sample data) */
	for (n = 0; n < hal_stream_element_count(&samp->fifo); n++) {
		rtapi_snprintf(buf, sizeof(buf), "sampler.%d.pin.%d", num, n);
		retval = hal_pin_new(__core_hal_user, buf, hal_stream_element_type(&samp->fifo, n), HAL_IN, (void **) pptr, comp_id);
		if (retval != 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: pin '%s' export failed\n", buf);
			return -EIO;
		}
		/* init the pin value */
		switch (hal_stream_element_type(&samp->fifo, n)) {
			case HAL_FLOAT:
				*(pptr->hfloat) = 0.0;
				usefp = 1;
				break;
			case HAL_BIT:
				*(pptr->hbit) = 0;
				break;
			case HAL_U32:
				*(pptr->hu32) = 0;
				break;
			case HAL_S32:
				*(pptr->hs32) = 0;
				break;
			default:
				break;
		}
		pptr++;
	}
	/* export update function */
	rtapi_snprintf(buf, sizeof(buf), "sampler.%d", num);
	retval = hal_export_funct(__core_hal_user, buf, sample, samp, usefp, 0, comp_id);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: function export failed\n");
		return retval;
	}

	return 0;
}

/* opencn - This part of code comes from the user space counterpart. */

static int sampler_user_init(sampler_connect_args_t *args, int major, int minor) {
	int ret;
	hal_user_t *hal_user;
	char comp_name[HAL_NAME_LEN + 1];

	hal_user = find_hal_user_by_dev(major, minor);
	if (!hal_user) {
		hal_user = (hal_user_t *) kmalloc(sizeof(hal_user_t), GFP_ATOMIC);
		if (!hal_user) {
            BUG();
		}

		memset(hal_user, 0, sizeof(hal_user_t));

		/* Get the current related PID. */
		hal_user->pid = current->pid;
		hal_user->major = major;
		hal_user->minor = minor;
		hal_user->channel = args->channel;

		add_hal_user(hal_user);
	}

	snprintf(comp_name, sizeof(comp_name), "halsampler%d", hal_user->pid);
	hal_user->comp_id = hal_init(hal_user, comp_name);

	hal_ready(hal_user, hal_user->comp_id);

	/* open shmem for user/RT comms (stream) */
	ret = hal_stream_attach(&hal_user->stream, hal_user->comp_id, SAMPLER_SHMEM_KEY + args->channel, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

int sampler_open(struct inode *inode, struct file *file) {
	return 0;
}

int sampler_release(struct inode *inode, struct file *filp) {
	return 0;
}

/*
 * Read a stream sample and returns the sample number.
 */
ssize_t sampler_read(struct file *filp, char __user *_sample, size_t len, loff_t *off) {
	int n, major, minor, num_pins;
	hal_user_t *hal_user;
	union hal_stream_data *rcv_data;
	int res;

	sampler_sample_t *sample = (sampler_sample_t *)_sample;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	hal_user = find_hal_user_by_dev(major, minor);
	BUG_ON(hal_user == NULL);

	num_pins = hal_stream_element_count(&hal_user->stream);
	rcv_data = kmalloc(sizeof(union hal_stream_data) * num_pins, GFP_ATOMIC);
	BUG_ON(rcv_data == NULL);

	/* hal_stream_wait_readable(&hal_user->stream); */

	res = hal_stream_read(&hal_user->stream, rcv_data, NULL);
	// lprintk("[SAMPLER] hal_stream_read = %d\n", res);
	if (res < 0) {
	    res = -1; // ssize_t can (should?) only represent -1 in the negative numbers
        goto out;
    }

	sample->n_pins = num_pins;
	for (n = 0; n < num_pins; n++) {
		switch (hal_stream_element_type(&hal_user->stream, n)) {
			case HAL_FLOAT:
				sample->pins[n].f = rcv_data[n].f;
				sample->pins[n].type = HAL_FLOAT;
				break;

			case HAL_BIT:
				sample->pins[n].b = rcv_data[n].b;
				sample->pins[n].type = HAL_BIT;
				break;

			case HAL_U32:
				sample->pins[n].u = rcv_data[n].u;
				sample->pins[n].type = HAL_U32;
				break;

			case HAL_S32:
				sample->pins[n].s = rcv_data[n].s;
				sample->pins[n].type = HAL_S32;
				break;

			default:
				/* better not happen */
				goto out;
		}
	}

	res = 0;

out:
	kfree(rcv_data);

	return res;

}

long sampler_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	int rc = 0, major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {

		case SAMPLER_IOCTL_CONNECT:

			BUG_ON(minor+1 > MAX_SAMPLERS);

			/* Pure kernel side init */
#warning Check if already present (initialized) ...
			rc = sampler_app_main(minor, (sampler_connect_args_t *) arg);

			if (rc) {
				printk("%s: failed to initialize...\n", __func__);
				goto out;
			}

			/* Initialization for this process instance. */
			rc = sampler_user_init((sampler_connect_args_t *) arg, major, minor);

			break;

		case SAMPLER_IOCTL_DISCONNECT:

			hal_user = find_hal_user_by_dev(major, minor);
			BUG_ON(hal_user == NULL);

			hal_stream_detach(&hal_user->stream);
			hal_exit(hal_user, hal_user->comp_id);

			sampler_app_exit();

			break;

	}
out:
	return rc;
}

struct file_operations sampler_fops = {
		.owner = THIS_MODULE,
		.open = sampler_open,
		.release = sampler_release,
		.unlocked_ioctl = sampler_ioctl,
		.read = sampler_read,
};

int sampler_comp_init(void) {

	int rc;

	printk("OpenCN: sampler subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(SAMPLER_DEV_MAJOR, SAMPLER_DEV_NAME, &sampler_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", SAMPLER_DEV_MAJOR);
		return rc;
	}

	return 0;
}

late_initcall(sampler_comp_init)

