/*
 * Copyright (C) 2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/fs.h>

#include <opencn/logfile.h>

#include <xenomai/rtdm/driver.h>

#include <opencn/opencn.h>
#include <opencn/uapi/opencn.h>

#include <soo/uapi/soo.h>

unsigned long volatile __cacheline_aligned_in_smp __jiffy_arch_data rtdm_jiffies = 0ul;

static rtdm_task_t rtdm_timer_task;

static void rtdm_timer_fn(void *arg) {

	while (1) {
		rtdm_task_wait_period(NULL);
		rtdm_jiffies++;
	}
}

int opencn_open(struct inode *inode, struct file *file) {
	return 0;
}

int opencn_release(struct inode *inode, struct file *filp) {
	return 0;
}

long opencn_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

	switch (cmd) {

		case OPENCN_IOCTL_LOGFILE_ON:
			lprintk("OpenCN: enabling in-kernel logging...\n");
			logfile_init();
			break;

		case OPENCN_IOCTL_LOGFILE_OFF:
			logfile_close();
			break;
	}

	return 0;
}

struct file_operations opencn_fops = {
	.owner = THIS_MODULE,
	.open = opencn_open,
	.release = opencn_release,
	.unlocked_ioctl = opencn_ioctl
};

int opencn_core_init(void) {
	int rc;

	lprintk("OpenCN: core subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(OPENCN_CORE_DEV_MAJOR, OPENCN_CORE_DEV_NAME, &opencn_fops);
	if (rc < 0) {
		printk("%s: Cannot obtain the major number %d\n", __func__, OPENCN_CORE_DEV_MAJOR);
		return rc;
	}

	/* Start incrementing the rtdm_jiffies accordingly. */
	rc = rtdm_task_init(&rtdm_timer_task, "rtdm_timer_task", rtdm_timer_fn, NULL, 50, (SECONDS(1) / CONFIG_RTDM_HZ));
	if (rc) {
		printk("%s: ERROR: Could not start rtdm timer task\n", __func__);
		BUG();
	}

	return 0;
}

subsys_initcall(opencn_core_init);
