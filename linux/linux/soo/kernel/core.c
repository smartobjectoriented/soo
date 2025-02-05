/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017, 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/device.h>

#include <asm/uaccess.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/injector.h>

#include <soo/core/sysfs.h>
#include <soo/core/migmgr.h>

#include <soo/guest_api.h>
#include <soo/soo.h> 
 
#define AGENCY_DEV_NAME "soo/core"
#define AGENCY_DEV_MAJOR 126

static struct soo_driver soo_core_driver;

/* SOO subsystem */
struct bus_type soo_subsys;

static struct device soo_dev;

/*
 * Perform a force terminate of ME in <ME_slotID>
 *
 */
static void force_terminate(unsigned int ME_slotID) {
	avz_hyp_t args;

	/* The ME may be ME_state_terminated after a cooperate callback */

	/* Asynchronous termination of the ME */
	if ((get_ME_state(ME_slotID) == ME_state_living) || (get_ME_state(ME_slotID) == ME_state_terminated))
		do_sync_dom(ME_slotID, DC_FORCE_TERMINATE);

	/* Then, final termination of the residual ME */
	if (get_ME_state(ME_slotID) == ME_state_terminated) {
                args.cmd = AVZ_KILL_ME;
                args.u.avz_kill_me_args.slotID = ME_slotID;

                avz_hypercall(&args);
        }
}

int agency_open(struct inode *inode, struct file *file) {
	return 0;
}

int agency_release(struct inode *inode, struct file *filp) {
	return 0;
}

long agency_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
        int ret = 0;
        agency_ioctl_args_t args;

        if ((copy_from_user(&args, (void *) arg, sizeof(agency_ioctl_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	switch (cmd) {

	case AGENCY_IOCTL_GET_ME_ID:
		args.value = (get_ME_id(args.slotID, (ME_id_t *) args.buffer) ? 0 : -1);
		break;

	case AGENCY_IOCTL_READ_SNAPSHOT:
		read_snapshot(args.slotID, args.buffer, (uint32_t *) &args.value);
		break;

	case AGENCY_IOCTL_WRITE_SNAPSHOT:
		ret = write_snapshot(args.buffer);
		break;

	case AGENCY_IOCTL_INJECT_ME:
		args.slotID = inject_ME(args.buffer, args.value);
		break;

	case AGENCY_IOCTL_FORCE_TERMINATE:
		force_terminate(args.slotID);
		break;
			
	case AGENCY_IOCTL_GET_ME_ID_ARRAY:
		get_ME_id_array((ME_id_t *) args.buffer);
		break;

	default:
		lprintk("%s: Unrecognized IOCTL: 0x%x\n", __func__, cmd);
		BUG();
	}

	if ((copy_to_user((void *) arg, &args, sizeof(agency_ioctl_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to send back args to userspace\n", __func__, __LINE__);
		BUG();
	}

	return ret;
}

struct file_operations agency_fops = {
    .owner = THIS_MODULE,
    .open = agency_open,
    .release = agency_release,
    .unlocked_ioctl = agency_ioctl,
};

/*  Driver core definition */

struct soo_device {
	struct device dev;
	struct resource resource;
	unsigned int id;
};

static int soo_probe(struct device *dev) {
	int rc;

	DBG("%s: probing...\n", __func__);

	/* Registering device */
	rc = register_chrdev(AGENCY_DEV_MAJOR, AGENCY_DEV_NAME, &agency_fops);
	if (rc < 0) {
		lprintk("Cannot obtain the major number %d\n", AGENCY_DEV_MAJOR);
		BUG();
	}

	return 0;
}

struct soo_driver {
	struct device_driver drv;
	int (*probe)(struct soo_device *);
	void (*remove)(struct soo_device *);
	int (*suspend)(struct soo_device *, pm_message_t);
	int (*resume)(struct soo_device *);
};

#define to_soo_device(d) container_of(d, struct soo_device, dev)
#define to_soo_driver(d) container_of(d, struct soo_driver, drv)

static int soo_match(struct device *dev, struct device_driver *drv) {
	/* Nothing at the moment, everything is matching... */

	return 1;
}

struct bus_type soo_subsys = {
    .name = "soo",
    .dev_name = "soo",
    .match = soo_match,
};

static int soo_driver_register(struct soo_driver *drv) {
	drv->drv.bus = &soo_subsys;

	return driver_register(&drv->drv);
}

void soo_driver_unregister(struct soo_driver *drv) {
	driver_unregister(&drv->drv);
}

/* Module init and exit */

static struct soo_driver soo_core_driver = {
	.drv = {
		.name = "soo_driver",
		.probe = soo_probe,
	},
};

static struct device soo_dev = {
    .bus = &soo_subsys,
};


static int agency_reboot_notify(struct notifier_block *nb, unsigned long code, void *unused)
{
	int slotID;
	ME_desc_t desc;

	lprintk("%s: !! Now terminating all Mobile Entities of this smart object...\n", __func__);

	for (slotID = 2; slotID < MAX_DOMAINS; slotID++) {

		/*
		 * Check if the ME slot is used or not in order to not
		   call force_terminate on an empty slot, which would cause an error.
		 */

		get_ME_desc(slotID, &desc);

		if (desc.size > 0) {
			lprintk("%s: terminating ME %d...\n", __func__, slotID);
			force_terminate(slotID);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block agency_reboot_nb = {
	.notifier_call = agency_reboot_notify,
};


extern int rtapp_main(void *args);

/*
 * This init function is called at a very late time in the boot process,
 * after all subsystems, devices, and late initcalls have been carried out.
 */
int agency_late_init_fn(void *args) {

	lprintk("SOO Agency last initialization part processing now...\n");

#ifdef CONFIG_SOO_RT_APP
	/* This thread will start various debugging testings possibly in RT domain .*/
	kernel_thread(rtapp_main, NULL, 0);
#endif /* CONFIG_SOO_RT_APP */

	/* Leaving the thread in a sane way... */
	do_exit(0);

	return 0;
}

int agency_init(void) {

	int rc;

	DBG("SOO Migration subsystem registering...\n");

	soo_sysfs_init();

	soo_guest_activity_init();

	rc = subsys_system_register(&soo_subsys, NULL);
	if (rc < 0) {
		printk("SOO subsystem register failed with rc = %d!\n", rc);
		return rc;
	}

	rc = soo_driver_register(&soo_core_driver);
	if (rc < 0) {
		printk("SOO driver register failed with rc = %d!\n", rc);
		return rc;
	}

	rc = device_register(&soo_dev);
	if (rc < 0) {
		printk("SOO device register failed with rc = %d!\n", rc);
		return rc;
	}

	DBG("SOO Migration subsystem registered...\n");

	register_reboot_notifier(&agency_reboot_nb);

	return 0;
}

void agency_exit(void) {
	unregister_chrdev(AGENCY_DEV_MAJOR, AGENCY_DEV_NAME);

	DBG0("SOO Agency exited.\n");
}

subsys_initcall(agency_init);
module_exit(agency_exit);
