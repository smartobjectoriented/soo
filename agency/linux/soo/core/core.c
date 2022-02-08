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

#if 0
#define ENABLE_LOGBOOL
#endif

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/string.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#ifndef CONFIG_X86
#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#else
#include <asm/pgtable-prot.h>
#endif
#endif

#include <soo/sooenv.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>

#include <soo/evtchn.h>
#include <soo/guest_api.h>
#include <soo/hypervisor.h>
#include <soo/vbstore.h>
#include <soo/vbus.h>
#include <soo/paging.h>

#include <soo/core/sysfs.h>
#include <soo/core/core.h>
#include <soo/core/device_access.h>
#include <soo/core/upgrader.h>

#include <soo/soolink/discovery.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/logbool.h>
#include <soo/uapi/injector.h>

#define AGENCY_DEV_NAME "soo/core"
#define AGENCY_DEV_MAJOR 126

#ifndef CONFIG_X86

static struct soo_driver soo_core_driver;

/* SOO subsystem */
struct bus_type soo_subsys;

static struct device soo_dev;

#endif /* !CONFIG_X86 */

#if 0 /* Debugging */
/*
 * Debugging purposes
 */
/* To get the address of the L2 page table from a L1 descriptor */

#define L1DESC_TYPE_MASK 0x3
#define L1DESC_TYPE_SECT 0x2
#define L1DESC_TYPE_PT 0x1

#define L1_PAGETABLE_ORDER 12
#define L2_PAGETABLE_ORDER 8

#define L1_PAGETABLE_ENTRIES (1 << L1_PAGETABLE_ORDER)
#define L2_PAGETABLE_ENTRIES (1 << L2_PAGETABLE_ORDER)

#define L1_PAGETABLE_SHIFT 20
#define L2_PAGETABLE_SHIFT 12

#define L1_PAGETABLE_SIZE (PAGE_SIZE << 2)

#define PAGE_SHIFT 12

#define __PAGE_MASK (~(PAGE_SIZE - 1))
#define L1_SECT_SIZE (0x100000)
#define L1_SECT_MASK (~(L1_SECT_SIZE - 1))
#define L2DESC_SMALL_PAGE_ADDR_MASK (~(PAGE_SIZE - 1))
/* To get the address of the L2 page table from a L1 descriptor */
#define L1DESC_L2PT_BASE_ADDR_SHIFT 10
#define L1DESC_L2PT_BASE_ADDR_OFFSET (1 << L1DESC_L2PT_BASE_ADDR_SHIFT)
#define L1DESC_L2PT_BASE_ADDR_MASK (~(L1DESC_L2PT_BASE_ADDR_OFFSET - 1))
#define l1pte_index(a) ((((u32)a) >> L1_PAGETABLE_SHIFT) & (L1_PAGETABLE_ENTRIES - 1))
#define l2pte_index(a) ((((u32)a) >> L2_PAGETABLE_SHIFT) & (L2_PAGETABLE_ENTRIES - 1))
#define l2pte_offset(l1pte, addr) (((u32 *)(((u32)phys_to_virt(*l1pte)) & L1DESC_L2PT_BASE_ADDR_MASK)) + l2pte_index(addr))

u32 virt_to_phys_pt(u32 vaddr) {
	u32 *l1pte, *l2pte;
	u32 offset;

	offset = vaddr & ~__PAGE_MASK;

	/* Get the L1 PTE. */
	l1pte = (u32 *) (((u32 *)swapper_pg_dir) + l1pte_index(vaddr));

	BUG_ON(!*l1pte);
	if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT) {
		printk("### l1pte content: %x\n", *l1pte);
		return *l1pte & L1_SECT_MASK;

	} else {
		printk("### l1pte content: %x\n", *l1pte);
		l2pte = l2pte_offset(l1pte, vaddr);
		printk("### -  l2pte content: %x\n", *l2pte);
		return (*l2pte & L2DESC_SMALL_PAGE_ADDR_MASK) | offset;
	}

}
#endif /* 0 */

#ifndef CONFIG_X86

/*  Driver core definition */

struct soo_device {
	struct device dev;
	struct resource resource;
	unsigned int id;
};

static int soo_probe(struct device *dev) {

	DBG("%s: probing...\n", __func__);

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

/*
 * Various sysfs attributes which may be used as debug helpers to retrieve values of
 * some debug variables
 */

/* Smart Object agencyUID management */
DEVICE_ATTR_RW(agencyUID);

/* Smart Object name management */
DEVICE_ATTR_RW(soo_name);

static struct attribute *soo_dev_attrs[] = {
    &dev_attr_agencyUID.attr,
    &dev_attr_soo_name.attr,
    NULL,
};

static struct attribute_group soo_dev_group = {
    .attrs = soo_dev_attrs,
};

const struct attribute_group *soo_dev_groups[] = {
    &soo_dev_group,
    NULL,
};

static struct device soo_dev = {
    .bus = &soo_subsys,
    .groups = soo_dev_groups
};

#endif /* !CONFIG_X86 */

int evtchn;

/* For testing purposes */

irqreturn_t dummy_interrupt(int irq, void *dev_id) {

	lprintk("### Got the interrupt %d on CPU: %d\n", irq, smp_processor_id());

	notify_remote_via_evtchn(evtchn);
	return IRQ_HANDLED;
};

static int agency_reboot_notify(struct notifier_block *nb, unsigned long code, void *unused)
{
	/* (OpenCN) Re-booting the agency does not require anything specific at the moment ... */

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

	/* At this point, we can start the Discovery process */
	sooenv_init();

#ifdef CONFIG_SOO_RT_APP
	/* This thread will start various debugging testings possibly in RT domain .*/
	kernel_thread(rtapp_main, NULL, 0);
#endif /* CONFIG_SOO_RT_APP */

	/* Leaving the thread in a sane way... */
	do_exit(0);

	return 0;
}

int agency_init(void) {
#ifndef CONFIG_X86
	int rc;
#endif

	DBG("SOO Migration subsystem registering...\n");

	soo_sysfs_init();

	soo_guest_activity_init();

#ifndef CONFIG_X86

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

	/* Initialize the agency UID and the dev caps bitmap */
	devaccess_init();

	register_reboot_notifier(&agency_reboot_nb);

#endif

	return 0;
}

void agency_exit(void) {
	unregister_chrdev(AGENCY_DEV_MAJOR, AGENCY_DEV_NAME);

	DBG0("SOO Agency exited.\n");
}

subsys_initcall(agency_init);
module_exit(agency_exit);
