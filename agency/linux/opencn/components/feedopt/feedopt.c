// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2020 Peter Lichard (peter.lichard@heig-vd.ch)
 */

/**
 * This file, 'feedopt.c', is the realtime part of feedopt.
 */

#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef CONFIG_ARM
#include <asm/neon.h>
#endif

#ifdef CONFIG_X86
#include <asm/fpu/api.h>
#endif

#include <opencn/hal/hal.h>			/* HAL public API decls */
#include <opencn/rtapi/rtapi.h>		/* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h> /* RTAPI realtime module decls */

#include <opencn/components/feedopt.h>

#include <opencn/ctypes/strings.h>

#include <opencn/strtox.h>

#include <opencn/rtapi/rtapi_errno.h>
#include <opencn/rtapi/rtapi_math.h>
#include <opencn/rtapi/rtapi_string.h>

#include "../lcct/lcct_internal.h"

static void fopt_rg_alloc(fopt_rg_t *rg, int capacity)
{
	rg->head = 0;
	rg->tail = 0;
	rg->capacity = capacity;
	rg->size = 0;
	rg->feedopt_data = kmalloc(sizeof(feedopt_sample_t) * capacity, GFP_ATOMIC);
}

static void fopt_rg_free(fopt_rg_t *rg)
{
	rg->capacity = 0;
	rg->size = 0;
	rg->tail = 0;
	rg->head = 0;
	kfree(rg->feedopt_data);
}

static int fopt_rg_push(fopt_rg_t *rg, feedopt_sample_t value)
{
	if (rg->size < rg->capacity) {
		rg->feedopt_data[rg->head++] = value;
		__sync_fetch_and_add(&rg->size, 1);
		if (rg->head == rg->capacity)
			rg->head = 0;
		return 1;
	}
	return 0;
}


static void fopt_rg_clear(fopt_rg_t *rg)
{
	rg->size = 0;
	rg->head = 0;
	rg->tail = 0;
}

/***********************************************************************
 *                STRUCTURES AND GLOBAL VARIABLES                       *
 ************************************************************************/

feedopt_hal_t *fopt_hal = NULL;
/* other globals */
static int comp_id; /* component ID */

#ifdef OPENCN_FORCE_UNDERRUN
static int debug_count = 0;
#endif

fopt_rg_t samples_queue;
feedopt_sample_t current_sample;

#define PIN(member) offsetof(feedopt_hal_t, member)
static const pin_def_t pin_def[] = {
    {HAL_FLOAT, HAL_OUT,  PIN(pin_sample_pos_out[0]), "feedopt.sample-0"},
    {HAL_FLOAT, HAL_OUT,  PIN(pin_sample_pos_out[1]), "feedopt.sample-1"},
    {HAL_FLOAT, HAL_OUT,  PIN(pin_sample_pos_out[2]), "feedopt.sample-2"},

    {HAL_FLOAT, HAL_OUT,  PIN(spindle_speed_out), "feedopt.spindle-target-speed"},

    {HAL_BIT, HAL_OUT,  PIN(rt_active), "feedopt.rt-active"},
    {HAL_BIT, HAL_IN,  PIN(us_active), "feedopt.us-active"},
    {HAL_BIT, HAL_IN,  PIN(rt_single_shot), "feedopt.rt-single-shot"},
    {HAL_U32, HAL_OUT,  PIN(pin_buffer_underrun_count), "feedopt.buffer-underrun-count"},
    {HAL_FLOAT, HAL_IN,  PIN(opt_per_second), "feedopt.opt-per-second"},
    {HAL_BIT, HAL_IN,  PIN(opt_rt_reset), "feedopt.opt-rt-reset"},
    {HAL_BIT, HAL_IN,  PIN(opt_rt_reset), "feedopt.opt-us-reset"},

    {HAL_BIT, HAL_IN,  PIN(commit_cfg), "feedopt.commit-cfg"},
    {HAL_U32, HAL_OUT,  PIN(queue_size), "feedopt.queue-size"},
    {HAL_U32, HAL_IN,  PIN(sampling_period_ns), "feedopt.sampling-period-ns"},
    {HAL_FLOAT, HAL_OUT,  PIN(current_u), "feedopt.current-u"},
    {HAL_BIT, HAL_OUT,  PIN(ready_out), "feedopt.ready"},
    {HAL_BIT, HAL_OUT,  PIN(finished_out), "feedopt.rt-finished"},
    {HAL_BIT, HAL_OUT,  PIN(underrun_out), "feedopt.rt-underrun"},
    {HAL_BIT, HAL_IN,  PIN(rt_start), "feedopt.rt-start"},
    {HAL_BIT, HAL_IN,  PIN(us_start), "feedopt.us-start"},
    {HAL_BIT, HAL_IN,  PIN(us_resampling_paused), "feedopt.resampling.paused"},
    {HAL_BIT, HAL_OUT,  PIN(rt_has_sample), "feedopt.rt-has-segment"},
    {HAL_FLOAT, HAL_IN,  PIN(manual_override), "feedopt.resampling.manual_override"},
    {HAL_FLOAT, HAL_IN,  PIN(auto_override), "feedopt.resampling.auto_override"},
    {HAL_S32, HAL_IN,  PIN(us_optimising_progress), "feedopt.optimising.progress"},
    {HAL_S32, HAL_IN,  PIN(us_optimising_count), "feedopt.optimising.count"},
    {HAL_S32, HAL_OUT,  PIN(rt_resampling_progress), "feedopt.resampling.progress"},
    {HAL_S32, HAL_OUT,  PIN(current_gcode_line), "feedopt.resampling.gcodeline"},

    HAL_PINDEF_END
};


FEEDOPT_STATE state = FEEDOPT_STATE_INACTIVE;

/***********************************************************************
 *                  LOCAL FUNCTION DECLARATIONS                         *
 ************************************************************************/

static int init_feedopt(int num, feedopt_hal_t *tmp_fifo);
static void feedopt_update(void *arg, long period);

extern void feedopt_update_fp(FEEDOPT_STATE state, long period);

/***********************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int feedopt_app_main(int n, feedopt_connect_args_t *args)
{
	int retval;

	comp_id = hal_init(__core_hal_user, "feedopt");
	if (comp_id < 0) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "FEEDOPT: ERROR: hal_init() failed\n");
		return -EINVAL;
	}

	retval = init_feedopt(n, fopt_hal);
	if (retval < 0)
		goto fail;

	hal_ready(__core_hal_user, comp_id);

	return 0;

fail:

	hal_exit(__core_hal_user, comp_id);

	return retval;
}


/***********************************************************************
 *            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
 ************************************************************************/

void feedopt_reset(void)
{
#ifdef OPENCN_FORCE_UNDERRUN
    debug_count = 0;
#endif
	*fopt_hal->rt_active = 0;
    *fopt_hal->rt_resampling_progress = 0;
	fopt_rg_clear(&samples_queue);
	*fopt_hal->pin_buffer_underrun_count = 0;
	state = FEEDOPT_STATE_INACTIVE;
}

static void feedopt_update(void *arg, long period)
{
#ifdef CONFIG_ARM
	kernel_neon_begin();
#endif

	feedopt_update_fp(state, period);

#ifdef CONFIG_ARM
	kernel_neon_end();
#endif
}

static int init_feedopt(int num, feedopt_hal_t *fopt)
{

	int retval, usefp;

	HAL_INIT_PINS(pin_def, comp_id, fopt_hal);

	usefp = 1;

	/* export update function */
	retval = hal_export_funct(__core_hal_user, "feedopt.update", feedopt_update, fopt, usefp, 0,
							  comp_id);
	if (retval != 0) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "FEEDOPT: ERROR: function export failed\n");
		return retval;
	}

	fopt_rg_alloc(&samples_queue, FEEDOPT_RT_QUEUE_SIZE);

	printk("[FEEDOPT]: Initialized\n");

	return 0;
}

/* opencn - This part of code comes from the user space counterpart. */

static int feedopt_user_init(feedopt_connect_args_t *args, int major, int minor) {
	hal_user_t *hal_user;
	char comp_name[HAL_NAME_LEN + 1];

	hal_user = find_hal_user_by_dev(major, minor);
	if (!hal_user) {
		hal_user = (hal_user_t *) kzalloc(sizeof(hal_user_t), GFP_ATOMIC);
		if (!hal_user)
			BUG();

		/* Get the current related PID. */
		hal_user->pid = current->pid;
		hal_user->major = major;
		hal_user->minor = minor;
		hal_user->channel = args->channel;

		add_hal_user(hal_user);
	}

	snprintf(comp_name, sizeof(comp_name), "halfeedopt%d", hal_user->pid);
	hal_user->comp_id = hal_init(hal_user, comp_name);

	hal_ready(hal_user, hal_user->comp_id);

	return 0;
}


int feedopt_open(struct inode *inode, struct file *file)
{
	return 0;
}

int feedopt_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * Read a stream feedopt_update and returns the feedopt_update number.
 */
ssize_t feedopt_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	return 0;
}

ssize_t feedopt_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	feedopt_sample_t p_curv = *(feedopt_sample_t *)buf;

#ifdef OPENCN_FORCE_UNDERRUN
    if (++debug_count > 100) {
        debug_count = 200;
        schedule();
        return PushStatus_TryAgain;
    }
#endif

    if (!fopt_rg_push(&samples_queue, p_curv)) {
		return PushStatus_TryAgain;
	}

	return PushStatus_Success;
}

long feedopt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0, major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {
	case FEEDOPT_IOCTL_CONNECT:
		BUG_ON(minor + 1 > 1);

/* Pure kernel side init */
#warning Check if already present (initialized) ...
		rc = feedopt_app_main(minor, (feedopt_connect_args_t *)arg);
		BUG_ON(rc);

		rc = feedopt_user_init((feedopt_connect_args_t *) arg, major, minor);
		BUG_ON(rc);

		break;

	case FEEDOPT_IOCTL_DISCONNECT:

		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);

		fopt_rg_free(&samples_queue);
		hal_exit(hal_user, hal_user->comp_id);

		hal_exit(__core_hal_user, comp_id);

		break;

	case FEEDOPT_IOCTL_RESET:
		/*
		 * when a reset is issued, the system could be in the slowdown phase, so we wait until it
		 * stops and reports that RT is inactive
		 */
		*fopt_hal->opt_rt_reset = 1;
		while (*fopt_hal->rt_active) {
			schedule();
		}
		feedopt_reset();

	break;

	}

	return 0;
}

struct file_operations feedopt_fops = {
	.owner = THIS_MODULE,
	.open = feedopt_open,
	.release = feedopt_release,
	.unlocked_ioctl = feedopt_ioctl,
	.read = feedopt_read,
	.write = feedopt_write,
};

int feedopt_comp_init(void)
{

	int rc;

	printk("OpenCN: feedopt subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(FEEDOPT_DEV_MAJOR, FEEDOPT_DEV_NAME, &feedopt_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", FEEDOPT_DEV_MAJOR);
		return rc;
	}

	return 0;
}

late_initcall(feedopt_comp_init)
