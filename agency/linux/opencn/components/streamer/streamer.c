/********************************************************************
 * Description:  streamer.c
 *               A HAL component that can be used to stream data
 *               from a file onto HAL pins at a specific realtime
 *               sample rate.
 *
 * Author: John Kasunich <jmkasunich at sourceforge dot net>
 * License: GPL Version 2
 *
 * Copyright (c) 2006 All rights reserved.
 *
 ********************************************************************/
/** This file, 'streamer.c', is the realtime part of a HAL component
 that allows numbers stored in a file to be "streamed" onto HAL
 pins at a uniform realtime sample rate.  When the realtime module
 is loaded, it creates a fifo in shared memory.  Then, the user
 space program 'hal_streamer' is invoked.  'hal_streamer' takes
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
 * streamer.N.cur-depth, streamer.N.empty and streamer.N.underruns are
 * updated even if streamer.N.enabled is set to false.
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

#include <opencn/rtapi/rtapi.h> /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h> /* RTAPI realtime module decls */
#include <opencn/hal/hal.h> /* HAL public API decls */
#include <opencn/components/streamer.h>

#include <opencn/hal/hal_priv.h>

#include <opencn/hal/hal.h>
#include <opencn/strtox.h>

#include <opencn/rtapi/rtapi_errno.h>
#include <opencn/rtapi/rtapi_string.h>

#include <opencn/ctypes/strings.h>

#include <opencn/uapi/hal.h>
#include <opencn/uapi/streamer.h>
#include <opencn/uapi/sampler.h>

#include <soo/uapi/console.h>

/***********************************************************************
 *                STRUCTURES AND GLOBAL VARIABLES                       *
 ************************************************************************/

/* this structure contains the HAL shared memory data for one streamer */

typedef struct {
	hal_stream_t fifo; /* pointer to user/RT fifo */
	hal_s32_t *curr_depth; /* pin: current fifo depth */
	hal_bit_t *empty; /* pin: underrun flag */
	hal_bit_t *enable; /* pin: enable streaming */
	hal_s32_t *underruns; /* pin: number of underruns */
	hal_bit_t *clock; /* pin: clock input */
	hal_s32_t *clock_mode; /* pin: clock mode */
	int myclockedge; /* clock edge detector */
	pin_data_t pins[HAL_STREAM_MAX_PINS];
} streamer_t;

typedef struct {
	hal_bit_t *load;
} streamer_usr_t;

/* other globals */
static int comp_id; /* component ID */
static int nstreamers;
static streamer_t *streamers;

/***********************************************************************
 *                  LOCAL FUNCTION DECLARATIONS                         *
 ************************************************************************/

static int init_streamer(int num, streamer_t *stream);
static void streamer_update(void *arg, long period);

/***********************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int streamer_app_main(int n, streamer_connect_args_t *args)
{
	int retval;

	comp_id = hal_init(__core_hal_user, "streamer");
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"STREAMER: ERROR: hal_init() failed\n");
		return -EINVAL;
	}

	streamers =
		hal_malloc(__core_hal_user, MAX_STREAMERS * sizeof(streamer_t));

	/* validate config info */

	retval = hal_stream_create(&streamers[n].fifo, comp_id,
				   STREAMER_SHMEM_KEY + n, args->depth,
				   args->cfg);
	if (retval < 0) {
		goto fail;
	}

	retval = init_streamer(n, &streamers[n]);

	hal_ready(__core_hal_user, comp_id);

	return 0;

fail:
	for (n = 0; n < nstreamers; n++)
		hal_stream_destroy(&streamers[n].fifo);

	hal_exit(__core_hal_user, comp_id);

	return retval;
}

void streamer_app_exit(void)
{
	int i;

	for (i = 0; i < nstreamers; i++)
		hal_stream_destroy(&streamers[i].fifo);

	hal_exit(__core_hal_user, comp_id);
}

/***********************************************************************
 *            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
 ************************************************************************/

static void streamer_update(void *arg, long period)
{
	streamer_t *str;
	pin_data_t *pptr;
	int n, doclk;
	int myclockedge;
	int depth;
	union hal_stream_data data[HAL_STREAM_MAX_PINS];
	union hal_stream_data *dptr;
	int num_pins;

	/* point at streamer struct in HAL shmem */
	str = arg;
	/* keep last two clock states to get all possible clock edges */
	myclockedge = str->myclockedge =
		((str->myclockedge << 1) | (*(str->clock) & 1)) & 3;

	/* are we enabled? - generate doclock if enabled and right mode  */
	doclk = 0;
	if (*(str->enable)) {
		doclk = 1;
		switch (*str->clock_mode) {
		/* clock-mode 0 means do clock if enabled */
		case 0:
			break;
			/* clock-mode 1 means enabled & falling edge */
		case 1:
			if (myclockedge != 2) {
				doclk = 0;
			}
			break;
			/* clock-mode 2 means enabled & rising edge */
		case 2:
			if (myclockedge != 1) {
				doclk = 0;
			}
			break;
			/* clock-mode 3 means enabled & both edges */
		case 3:
			if ((myclockedge == 0) | (myclockedge == 3)) {
				doclk = 0;
			}
			break;
		default:
			break;
		}
	}
	/* pint at HAL pins */
	pptr = str->pins;

	/* point at user/RT fifo in other shmem */
	depth = hal_stream_depth(&str->fifo);
	*(str->curr_depth) = depth;
	*(str->empty) = depth == 0;
	if (!doclk) 
		/* done - output pins retain current values */
		return;
	if (depth == 0) {
		/* increase underrun only for valid clock*/
		(*str->underruns)++;
		return;
	}

	if (hal_stream_read(&str->fifo, data, NULL) < 0) {
		/* should not happen (single reader invariant) */
		(*str->underruns)++;
		opencn_cprintf(OPENCN_COLOR_BRED, "[STREAMER] Underrun\n");
		return;
	}
	dptr = data;
	num_pins = hal_stream_element_count(&str->fifo);

	/* copy data from fifo to HAL pins */
	for (n = 0; n < num_pins; n++) {
		switch (hal_stream_element_type(&str->fifo, n)) {
		case HAL_FLOAT:
			*(pptr->hfloat) = dptr->f;
			break;
		case HAL_BIT:
			if (dptr->b) {
				*(pptr->hbit) = 1;
			} else {
				*(pptr->hbit) = 0;
			}
			break;
		case HAL_U32:
			*(pptr->hu32) = dptr->u;
			break;
		case HAL_S32:
			*(pptr->hs32) = dptr->s;
			break;
		default:
			break;
		}
		dptr++;
		pptr++;
	}
}

static int init_streamer(int num, streamer_t *str)
{
	int retval, n, usefp;
	pin_data_t *pptr;
	char buf[HAL_NAME_LEN + 1];

	/* export "standard" pins and params */
	retval = hal_pin_bit_newf(__core_hal_user, HAL_OUT, &(str->empty),
				  comp_id, "streamer.%d.empty", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"STREAMER: ERROR: 'empty' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_bit_newf(__core_hal_user, HAL_IN, &(str->enable),
				  comp_id, "streamer.%d.enable", num);
	if (retval != 0) {
		rtapi_print_msg(
			RTAPI_MSG_ERR,
			"STREAMER: ERROR: 'enable' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_s32_newf(__core_hal_user, HAL_OUT, &(str->curr_depth),
				  comp_id, "streamer.%d.curr-depth", num);
	if (retval != 0) {
		rtapi_print_msg(
			RTAPI_MSG_ERR,
			"STREAMER: ERROR: 'curr_depth' pin export failed\n");
		return -EIO;
	}
	retval = hal_pin_s32_newf(__core_hal_user, HAL_IO, &(str->underruns),
				  comp_id, "streamer.%d.underruns", num);
	if (retval != 0) {
		rtapi_print_msg(
			RTAPI_MSG_ERR,
			"STREAMER: ERROR: 'underruns' pin export failed\n");
		return -EIO;
	}

	retval = hal_pin_bit_newf(__core_hal_user, HAL_IN, &(str->clock),
				  comp_id, "streamer.%d.clock", num);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"STREAMER: ERROR: 'clock' pin export failed\n");
		return -EIO;
	}

	retval = hal_pin_s32_newf(__core_hal_user, HAL_IN, &(str->clock_mode),
				  comp_id, "streamer.%d.clock-mode", num);
	if (retval != 0) {
		rtapi_print_msg(
			RTAPI_MSG_ERR,
			"STREAMER: ERROR: 'clock_mode' pin export failed\n");
		return -EIO;
	}

	/* init the standard pins and params */
	*(str->empty) = 1;
	*(str->enable) = 1;
	*(str->curr_depth) = 0;
	*(str->underruns) = 0;
	*(str->clock_mode) = 0;
	pptr = str->pins;
	usefp = 0;

	/* export user specified pins (the ones that stream data) */
	for (n = 0; n < hal_stream_element_count(&str->fifo); n++) {
		rtapi_snprintf(buf, sizeof(buf), "streamer.%d.pin.%d", num, n);
		retval = hal_pin_new(__core_hal_user, buf,
				     hal_stream_element_type(&str->fifo, n),
				     HAL_OUT, (void **)pptr, comp_id);
		if (retval != 0) {
			rtapi_print_msg(
				RTAPI_MSG_ERR,
				"STREAMER: ERROR: pin '%s' export failed\n",
				buf);
			return -EIO;
		}
		/* init the pin value */
		switch (hal_stream_element_type(&str->fifo, n)) {
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

	rtapi_snprintf(buf, sizeof(buf), "streamer.%d", num);
	retval = hal_export_funct(__core_hal_user, buf, streamer_update, str,
				  usefp, 0, comp_id);
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"STREAMER: ERROR: function export failed\n");

		return retval;
	}

	return 0;
}

/* opencn - This part of code comes from the user space counterpart. */

static int streamer_user_init(streamer_connect_args_t *args, int major,
			      int minor)
{
	int ret;
	hal_user_t *hal_user;
	char comp_name[HAL_NAME_LEN + 1];
	streamer_usr_t *streamer_state;

	hal_user = find_hal_user_by_dev(major, minor);
	if (!hal_user) {
		hal_user =
			(hal_user_t *)kmalloc(sizeof(hal_user_t), GFP_ATOMIC);
		if (!hal_user)
			BUG();

		memset(hal_user, 0, sizeof(hal_user_t));

		/* Get the current related PID. */
		hal_user->pid = current->pid;
		hal_user->major = major;
		hal_user->minor = minor;
		hal_user->channel = args->channel;

		add_hal_user(hal_user);
	}

	snprintf(comp_name, sizeof(comp_name), "halstreamer%d", hal_user->pid);
	hal_user->comp_id = hal_init(hal_user, comp_name);

	streamer_state = hal_malloc(hal_user, sizeof(streamer_usr_t));
	if (!streamer_state)
		BUG();

	streamer_state->load = 0;

	/* User-defined streamer PINs */
	ret = hal_pin_bit_newf(hal_user, HAL_IN, &(streamer_state->load),
			       hal_user->comp_id, "streamer.%d.load", 0);

	if (ret != 0) {
		rtapi_print_msg(
			RTAPI_MSG_ERR,
			"STREAMER: ERROR: private 'load' pin export failed\n");
		return -EIO;
	}

	hal_user->priv = streamer_state;

	hal_ready(hal_user, hal_user->comp_id);

	/* open shmem for user/RT comms (stream) */
	ret = hal_stream_attach(&hal_user->stream, hal_user->comp_id,
				STREAMER_SHMEM_KEY + args->channel, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

int streamer_open(struct inode *inode, struct file *file)
{
	return 0;
}

int streamer_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t streamer_write(struct file *filp, const char __user *_sample,
		       size_t len, loff_t *off)
{
	int i, major, minor, num_pins;
	hal_user_t *hal_user;
	union hal_stream_data *src_data;
	union hal_stream_data *dptr;
	int ret;

	const sampler_sample_t *sample = (const sampler_sample_t *)_sample;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	hal_user = find_hal_user_by_dev(major, minor);
	BUG_ON(hal_user == NULL);

	num_pins = hal_stream_element_count(&hal_user->stream);
	src_data =
		kmalloc(sizeof(union hal_stream_data) * num_pins, GFP_ATOMIC);
	if (!src_data)
		BUG();

	for (i = 0; i < sample->n_pins; i++) {
		dptr = &src_data[i];

		switch (hal_stream_element_type(&hal_user->stream, i)) {
		case HAL_FLOAT:
			dptr->f = sample->pins[i].f;
			break;

		case HAL_BIT:
			dptr->b = sample->pins[i].b;
			break;

		case HAL_U32:
			dptr->u = sample->pins[i].u;
			break;

		case HAL_S32:
			dptr->s = sample->pins[i].s;
			break;

		default:
			opencn_cprintf(OPENCN_COLOR_RED,
				       "Streamer: Wrong type\n");
			BUG();
		}
	}
	/* good data, keep it */
	/* hal_stream_wait_writable(&hal_user->stream); */
	ret = hal_stream_write(&hal_user->stream, src_data);

	kfree(src_data);

	return ret;
}

long streamer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0, major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {
	case STREAMER_IOCTL_CONNECT:

		BUG_ON(minor + 1 > MAX_STREAMERS);

		/* Pure kernel side init */
#warning Check if already present (initialized) ...
		rc = streamer_app_main(minor, (streamer_connect_args_t *)arg);

		if (rc) {
			printk("%s: failed to initialize...\n", __func__);
			goto out;
		}

		/* Initialization for this process instance. */
		rc = streamer_user_init((streamer_connect_args_t *)arg, major,
					minor);

		break;

	case STREAMER_IOCTL_DISCONNECT:

		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);

		hal_stream_detach(&hal_user->stream);

		break;

	case STREAMER_IOCTL_CLEAR_FIFO:

		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);

		hal_stream_clear_fifo(&hal_user->stream);

		break;
	}
out:
	return rc;
}

struct file_operations streamer_fops = {
	.owner = THIS_MODULE,
	.open = streamer_open,
	.release = streamer_release,
	.unlocked_ioctl = streamer_ioctl,
	.write = streamer_write,
};

int streamer_comp_init(void)
{
	int rc;

	printk("OpenCN: streamer subsystem initialization.\n");

	/* Registering device */
        
	rc = register_chrdev(STREAMER_DEV_MAJOR, STREAMER_DEV_NAME,
			     &streamer_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n",
		       STREAMER_DEV_MAJOR);
		return rc;
	}

	return 0;
}

late_initcall(streamer_comp_init)
