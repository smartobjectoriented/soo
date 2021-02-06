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

//#define VERBOSE

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

#include <asm/io.h>
#include <asm/uaccess.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#endif

#include <asm/cacheflush.h>

#include <soo/netsimul.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/debug/dbgvar.h>
#include <soo/evtchn.h>
#include <soo/guest_api.h>
#include <soo/hypervisor.h>
#include <soo/vbstore.h>
#include <soo/vbus.h>

#include <soo/core/sysfs.h>
#include <soo/core/core.h>
#include <soo/core/device_access.h>
#include <soo/core/upgrader.h>

#include <soo/soolink/discovery.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/logbool.h>
#include <soo/uapi/injector.h>

#define AGENCY_DEV_NAME "soo/core"
#define AGENCY_DEV_MAJOR 126

/* Fixed size for the header of the ME buffer frame (max.) */
#define ME_EXTRA_BUFFER_SIZE (1024 * 1024)

#ifdef CONFIG_ARM

/*
 * Used to store ioctl args/buffers
 * Cannot be stored on the local stack since it is to big.
 */
static uint8_t buffer[32 * 1024]; /* 32 Ko */

static struct soo_driver soo_core_driver;

/* SOO subsystem */
struct bus_type soo_subsys;

static struct device soo_dev;

/* Agency callback implementation */

/*
 * Perform a force terminate of ME in <ME_slotID>
 *
 * Possibly, the target ME may not accept to be terminated. In this case,
 * we exit the function.
 *
 * Returns the state of the target ME.
 */
ME_state_t force_terminate(unsigned int ME_slotID) {
	int rc;

	if (get_ME_state(ME_slotID) == ME_state_living)
		do_sync_dom(ME_slotID, DC_FORCE_TERMINATE);

	if ((get_ME_state(ME_slotID) == ME_state_dormant) || (get_ME_state(ME_slotID) == ME_state_terminated)) {

		rc = soo_hypercall(AVZ_KILL_ME, NULL, NULL, &ME_slotID, NULL);
		if (rc != 0) {
			printk("%s: failed to terminate the ME by the hypervisor (%d)\n", __func__, rc);
			return rc;
		}

		/* That's the end ! */
		return ME_state_dead;
	}

	return get_ME_state(ME_slotID);
}

#endif /* CONFIG_ARM */

/* Agency ctl domcalls operations */

/*
 * Dump a memory page based on a physical address belonging to
 * the linear address space of the kernel.
 */
void dumpPage(unsigned int phys_addr, unsigned int size) {
	int i, j;

	lprintk("%s: phys_addr: %x\n\n", __func__, phys_addr);

	for (i = 0; i < size; i += 16) {
		lprintk(" [%x]: ", i);
		for (j = 0; j < 16; j++) {
			lprintk("%02x ", *((unsigned char *) __va(phys_addr)));
			phys_addr++;
		}
		lprintk("\n");
	}
}

#ifdef CONFIG_ARM

/*
 * Set the personality.
 */
static int ioctl_set_personality(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	soo_personality_t pers;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	pers = (soo_personality_t)args.value;

	if ((pers != SOO_PERSONALITY_INITIATOR) && (pers != SOO_PERSONALITY_TARGET) &&
	    (pers != SOO_PERSONALITY_SELFREFERENT)) {
		lprintk("Agency: %s:%d Invalid personality value (%d)\n", __func__, __LINE__, pers);
		BUG();
	}

	soo_set_personality(pers);

	return 0;
}

/*
 * Get the personality.
 */
static int ioctl_get_personality(unsigned long arg) {
	int rc;
	agency_tx_args_t args;

	args.value = soo_get_personality();

	if ((rc = copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to transmit args to userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

/**
 * Initialize the migration process of a ME.
 *
 * The process starts with the execution of pre_propagate, which might decide to kill (remove) the ME.
 * This is useful to control the propagation of the ME when necessary.
 * Furthermore, if the ME is dormant, pre_propagate is still called, but the rest of the process is skipped.
 * To let the user space aware of the state, a return value is put in args.value as follows:
 * - (-1) means the ME is dead (has disappeared) or can not pursue its propagation.
 * - (0) means the ME is dormant
 * - (1) means the ME is ready be activated
 */
static int ioctl_initialize_migration(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	soo_personality_t pers = soo_get_personality();
	int propagate = 0;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	if (pers == SOO_PERSONALITY_INITIATOR) {

		if ((rc = soo_hypercall(AVZ_MIG_PRE_PROPAGATE, NULL, NULL, &args.ME_slotID, &propagate)) != 0) {
			lprintk("Agency: %s:%d Failed to trigger pre-propagate callback (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		/* Set a return value so that the caller can decide what to do. */
		if (get_ME_state(args.ME_slotID) == ME_state_dead) {

			/* Just make sure that the ME has not triggered any vbstore entry creation. */
			/* Remove all associated entries. */

#warning Remove all vbstore entries....

			args.value = -1;
			goto out;
		}

#ifdef VERBOSE
		lprintk("%s: returned propagate value = %d\n", __func__, propagate);
#endif

		if (!propagate) {
			args.value = -1;
			goto out;
		}

		if (get_ME_state(args.ME_slotID) == ME_state_dormant) {
			args.value = 0;
			goto out;
		}

		do_sync_dom(args.ME_slotID, DC_PRE_SUSPEND);

		/* Set the ME in suspended state */
		set_ME_state(args.ME_slotID, ME_state_suspended);

		vbus_suspend_devices(args.ME_slotID);

		do_sync_dom(args.ME_slotID, DC_SUSPEND);
	}

	if ((rc = soo_hypercall(AVZ_MIG_INIT, NULL, NULL, &args.ME_slotID, &pers)) != 0) {
		lprintk("Agency: %s:%d Failed to initialize migration (%d)\n", __func__, __LINE__, rc);
		return rc;
	}

	/* Ready to be migrated */
	args.value = 0;
out:
	if ((rc = copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

/**
 * Get an available ME slot from the hypervisor for a ME with a specific size (<size>).
 * If no slot is available, the value field of the agency_tx_args_t structure will be set to -1.
 */
static int ioctl_get_ME_free_slot(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	int val;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		return rc;
	}

	val = args.value;

	DBG("Agency: trying to get a slot for a ME of %d bytes ...\n", val);

	if ((rc = soo_hypercall(AVZ_GET_ME_FREE_SLOT, NULL, NULL, &val, NULL)) != 0) {
		lprintk("Agency: %s:%d Failed to get ME slot from hypervisor (%d)\n", __func__, __LINE__, rc);
		return rc;
	}

	args.ME_slotID = val;

	if ((rc = copy_to_user((void *) arg, (const void *) &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		return rc;
	}

	if (val == -1) {
		DBG0("Agency: no slot available anymore ...");
	} else {
		DBG("Agency: ME slot ID %d available.\n", val);
	}

	return 0;
}

/**
 * Retrieve the ME descriptor including the SPID, the state and the SPAD.
 */
static int ioctl_get_ME_desc(unsigned int arg) {
	int rc;
	agency_tx_args_t args;
	ME_desc_t ME_desc;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	DBG("ME_slotID=%d\n", args.ME_slotID);

	get_ME_desc(args.ME_slotID, &ME_desc);

	if ((rc = copy_to_user(args.buffer, &ME_desc, sizeof(ME_desc_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		BUG();
	}

	if ((rc = copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

static int ioctl_write_snapshot(unsigned long arg) {
	ME_desc_t ME_desc;
	agency_tx_args_t args;
	void *target;
	ME_info_transfer_t *ME_info_transfer;
	uint32_t crc32;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(args.ME_slotID, &ME_desc);

	/* Beginning of the ME_buffer */
	ME_info_transfer = (ME_info_transfer_t *) args.buffer;

	/* Check the CRC32 for consistency purposes. */
	crc32 = xcrc32(args.buffer + sizeof(ME_info_transfer_t), args.value - sizeof(ME_info_transfer_t), 0xffffffff);
	soo_log("[soo:core] Computed CRC32 of the received snapshot: %x / embedded crc32 value: %x / Size: %x\n", crc32, ME_info_transfer->crc32, args.value);

	BUG_ON(crc32 != ME_info_transfer->crc32);

	/* Retrieve the info related to the migration structure */
	memcpy(buffer, args.buffer + sizeof(ME_info_transfer_t), ME_info_transfer->size_mig_structure);

	if (soo_hypercall(AVZ_MIG_WRITE_MIGRATION_STRUCT, buffer, NULL, NULL, NULL) < 0) {
		lprintk("Agency: %s:%d Failed to write migration struct.\n", __func__, __LINE__);
		BUG();
	}

	/* We got the pfn of the local destination for this ME, therefore... */
	target = __arm_ioremap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size, MT_MEMORY_RWX_NONCACHED);
	BUG_ON(target == NULL);

	/* Finally, perform the copy */
	memcpy(target, (void *) (args.buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure), ME_desc.size);

	/* Relase the map used to copy the ME to its final location */
	iounmap(target);

	cache_flush_all();

	/* Release the buffer */

	return 0;
}

/*
 * Retrieve a valid (user space) address to the ME snapshot which has been previously
 * read with the READ_SNAPSOT ioctl.
 *
 * In tx_args_t <args>, the following fields are used as follows:
 *
 * @buffer:	pointer to the vmalloc'd memory (not be used in the user space, but will be used in following calls
 * @ME_slotID: 	the *size* of the contents to be copied.
 * @value: 	the target user space address the ME has to be copied to
 */
static void ioctl_get_ME_snapshot(unsigned long arg) {
	agency_tx_args_t args;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Awful usage of these fields, the name will have to evolve... */
	memcpy((void *) args.value, args.buffer, args.ME_slotID);
}

/*
 * Read a ME snapshot for migration or saving.
 * Currently, the ME is read and stored in a vmalloc'd memory area.
 * It will be preferable in the future to have a memory allocated and handled by
 * the user space application (avoiding restriction access and so on).
 *
 * In tx_args_t <args>, the following fields are used as follows:
 *
 * @buffer:	pointer to the vmalloc'd memory (not be used in the user space, but will be used in following calls
 * @ME_slotID: 	the slotID which leads to retrieving the ME descriptor
 * @value: 	the size including ME size + additional transfer information
 */
static int ioctl_read_snapshot(unsigned long arg) {
	ME_desc_t ME_desc;
	agency_tx_args_t args;
	ME_info_transfer_t *ME_info_transfer;
	void *ME_buffer;
	void *source;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(args.ME_slotID, &ME_desc);

	/*
	 * Prepare a buffer to store the ME and additional header information like migration structure and transfer information.
	 * The buffer must be free'd once it has been sent out (by the DCM).
	 */
	ME_buffer = __vmalloc(ME_desc.size + ME_EXTRA_BUFFER_SIZE, GFP_HIGHUSER | __GFP_ZERO, PAGE_SHARED | PAGE_KERNEL);
	BUG_ON(ME_buffer == NULL);

	/* Beginning of the ME buffer to transmit - We start with the information transfer. */
	ME_info_transfer = (ME_info_transfer_t *) ME_buffer;
	ME_info_transfer->ME_size = ME_desc.size;

	if ((soo_hypercall(AVZ_MIG_READ_MIGRATION_STRUCT, buffer, NULL, &args.ME_slotID, &args.value)) < 0) {
		lprintk("Agency: %s:%d Failed to read migration struct.\n", __func__, __LINE__);
		BUG();
	}

	/* Store the migration structure within the ME buffer */
	memcpy(ME_buffer + sizeof(ME_info_transfer_t), buffer, args.value);

	/* Keep the size of migration structure */
	ME_info_transfer->size_mig_structure = args.value;

	/* Finally, store the ME in this buffer. */
	source = ioremap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(source == NULL);

	memcpy(ME_buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure, source, ME_desc.size);

	iounmap(source);

	cache_flush_all();

	/* Compute the crc32 of the snapshot */

	args.buffer = ME_buffer;
	args.value = sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure + ME_desc.size;

	/* The CRC32 is done over the bytes right after the ME_info_transfer structure since the result will be placed within this structure. */
	ME_info_transfer->crc32 = xcrc32(args.buffer + sizeof(ME_info_transfer_t), args.value - sizeof(ME_info_transfer_t), 0xffffffff);

	soo_log("[soo:core] Computed CRC32 of the current snapshot: %x / Size: %x\n", ME_info_transfer->crc32, args.value);

	if ((copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}


	return 0;
}

/**
 * Initiate the last stage of the migration process of a ME, so called "migration
 * finalization".
 */
static int ioctl_finalize_migration(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	soo_personality_t pers = soo_get_personality();
	int ME_slotID, ME_state;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		return rc;
	}

	if (pers == SOO_PERSONALITY_SELFREFERENT) {
		ME_slotID = args.ME_slotID;

		DBG("Unpause the ME (slot %d)...\n", ME_slotID);

		/*
		 * During the unpause operation, we take the opportunity to pass the pfn of the shared page used for exchange
		 * between the ME and VBstore.
		 */
		avz_ME_unpause(ME_slotID, virt_to_pfn((unsigned int) __vbstore_vaddr[ME_slotID]));

		/*
		 * Now, we must wait for the ME to set its state to ME_state_preparing to pause it. We'll then be able
		 * to perform a pre-activate callback on it.
		 */
		while (1) {
			schedule();

			if (get_ME_state(ME_slotID) == ME_state_preparing) {
				DBG("ME now paused, continuing...\n");
				break;
			}
		}

		/* Pause the ME */
		avz_ME_pause(ME_slotID);

		/*
		 * Now we pursue with a call to pre-activate callback to
		 * see if the Smart Object has the necessary devcaps. To do that,
		 * we ask the ME to decide.
		 */
		if ((rc = soo_hypercall(AVZ_MIG_PRE_ACTIVATE, NULL, NULL, &ME_slotID, NULL)) != 0) {
			lprintk("Agency: %s:%d Failed to trigger pre-activate callback (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		/* Check if the pre-activate callback has changed the ME state */
		if ((get_ME_state(ME_slotID) == ME_state_dead) || (get_ME_state(ME_slotID) == ME_state_dormant))
			return 0;

		if ((rc = soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &args.ME_slotID, &pers)) < 0) {
			lprintk("Agency: %s:%d Failed to finalize migration (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		ME_state = get_ME_state(args.ME_slotID);

		if ((ME_state != ME_state_dead) && (ME_state != ME_state_dormant)) {

			/* Tell the ME that it can go further */
			set_ME_state(ME_slotID, ME_state_booting);

			DBG("Unpause the ME and waiting boot completion...\n");

			/* Unpause the ME */
			avz_ME_unpause(ME_slotID, virt_to_pfn((unsigned int) __vbstore_vaddr[ME_slotID]));

			/* Wait for all backend/frontend initialized. */
			wait_for_completion(&backend_initialized);

			DBG("The ME is now living, continuing the injection...\n");

			ME_state = get_ME_state(args.ME_slotID);

			DBG("Putting ME domid %d in state living...\n", args.ME_slotID);
			set_ME_state(args.ME_slotID, ME_state_living);
		}

	} else {

		DBG0("SOO migration subsys: Entering post migration tasks...\n");

		if ((rc = soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &args.ME_slotID, &pers)) < 0) {
			lprintk("Agency: %s:%d Failed to finalize migration (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		DBG0("Call to AVZ_MIG_FINAL terminated\n");

		ME_state = get_ME_state(args.ME_slotID);

		if ((ME_state != ME_state_dead) && (ME_state != ME_state_dormant)) {
			DBG0("Pinging ME for DC_RESUME...\n");
			do_sync_dom(args.ME_slotID, DC_RESUME);

			DBG("Resuming all devices (resuming from backend devices) on domain %d...\n", args.ME_slotID);
			vbus_resume_devices(args.ME_slotID);

			DBG("Pinging ME %d for DC_POST_ACTIVATE...\n", args.ME_slotID);
			do_sync_dom(args.ME_slotID, DC_POST_ACTIVATE);

			DBG("Putting ME domid %d in state living...\n", args.ME_slotID);
			set_ME_state(args.ME_slotID, ME_state_living);
		}
	}

	return 0;
}

static int ioctl_get_upgrade_image(unsigned long arg) {
	upgrader_ioctl_recv_args_t args;

	args.size = devaccess_get_upgrade_size();
	args.ME_slotID = devaccess_get_upgrade_ME_slotID();

	/* Check if an upgrade image is available */
	if (args.size == 0) {
		return 1;
	}

	if ((copy_to_user((void *) arg, &args, sizeof(upgrader_ioctl_recv_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

static int ioctl_store_versions(unsigned long arg) {
	upgrade_versions_args_t version_args;

	if (copy_from_user(&version_args, (const void *) arg, sizeof(upgrade_versions_args_t)) != 0) {
		printk("Agency: %s:%d failed to retrieve args from userspace\n", __func__, __LINE__);
		return -EFAULT;
	}

	vbus_printf(VBT_NIL, "/soo", "itb-version", "%u", version_args.itb);
	vbus_printf(VBT_NIL, "/soo", "uboot-version", "%u", version_args.uboot);
	vbus_printf(VBT_NIL, "/soo", "rootfs-version", "%u", version_args.rootfs);

	return 0;
}

/*
 * Force terminate the execution of a specific ME
 *
 * Return 0 in case of success, or 1 if the target ME cannot be terminated.
 */
int soo_force_terminate(unsigned long arg) {
	unsigned int ME_slotID;
	int rc;

	/* Get arguments from user space */
	if (copy_from_user(&ME_slotID, (const void *) arg, sizeof(ME_slotID)) != 0) {
		printk("Agency: %s:%d failed to retrieve args from userspace\n", __func__, __LINE__);
		return -EFAULT;
	}

	rc = force_terminate(ME_slotID);

	DBG0("Completed\n");

	return rc;
}

int agency_open(struct inode *inode, struct file *file) {
	return 0;
}

int agency_release(struct inode *inode, struct file *filp) {
	return 0;
}

#endif /* CONFIG_ARM */

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

#ifdef CONFIG_ARM

long agency_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	int rc = 0;
	unsigned int i1, i2;
	unsigned int *ptr;
	pte_t *pte;
	struct task_struct *p;
	unsigned int phys_addr;
	dump_page_t dump_page;
	unsigned int ME_slotID;

	switch (cmd) {

	case AGENCY_IOCTL_SET_PERSONALITY:
		if ((rc = ioctl_set_personality(arg)) < 0) {
			lprintk("%s: SET_PERSONALITY error (%d)\n", __func__, rc);
			BUG();
		}
		break;

	case AGENCY_IOCTL_GET_PERSONALITY:
		if ((rc = ioctl_get_personality(arg)) < 0) {
			lprintk("%s: GET_PERSONALITY error (%d)\n", __func__, rc);
			BUG();
		}
		break;

	case AGENCY_IOCTL_INIT_MIGRATION:
		if ((rc = ioctl_initialize_migration(arg)) < 0) {
			printk("%s: INIT_MIGRATION error (%d)\n", __func__, rc);
			BUG();
		}
		break;

	case AGENCY_IOCTL_GET_ME_FREE_SLOT:
		if ((rc = ioctl_get_ME_free_slot(arg)) < 0) {
			lprintk("%s: GET_ME_FREE_SLOT error (%d)\n", __func__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_GET_ME_DESC:
		if ((rc = ioctl_get_ME_desc(arg)) < 0) {
			lprintk("%s: GET_ME_DESC error (%d)\n", __func__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_READ_SNAPSHOT:
		if ((rc = ioctl_read_snapshot(arg)) < 0) {
			lprintk("%s: READ SNAPSHOT error (%d)\n", __func__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_WRITE_SNAPSHOT:
		if ((rc = ioctl_write_snapshot(arg)) < 0) {
			lprintk("%s: WRITE SNAPSHOT error (%d)\n", __func__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_FINAL_MIGRATION:
		if ((rc = ioctl_finalize_migration(arg)) < 0) {
			printk("%s: FINAL_MIGRATION error (%d)\n", __func__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_INJECT_ME:
		if ((rc = ioctl_inject_ME(arg)) < 0) {
			lprintk("%s: INJECT_ME error (%d)\n", __func__, rc);
			BUG();
		}

		break;

		/* Post-migration activities */

	case AGENCY_IOCTL_FORCE_TERMINATE:
		rc = soo_force_terminate(arg);
		if (rc < 0) {
			printk("%s: FORCE TERMINATE ME failed (%d)\n", __FUNCTION__, rc);
			BUG();
		}

		break;

	case AGENCY_IOCTL_LOCALINFO_UPDATE:
		/* Get arguments from user space */
		if (copy_from_user(&ME_slotID, (const void *)arg, sizeof(ME_slotID)) != 0) {
			printk("SOO Agency: %s failed to get args from userspace!\n", __func__);
			return -EFAULT;
		}

		if (get_ME_state(ME_slotID) == ME_state_living) {
			/* Propagating the DC event to the target ME */
			do_sync_dom(ME_slotID, DC_LOCALINFO_UPDATE);
		}

		break;

	case AGENCY_IOCTL_PICK_NEXT_UEVENT:
		/* Suspend this thread until a SOO event (via uevent) is fired */
		rc = pick_next_uevent();
		break;

	case AGENCY_IOCTL_READY:
		up(&usr_feedback);
		break;

	case AGENCY_IOCTL_DUMP:
		/* Get arguments from user space */
		if (copy_from_user(&dump_page, (const void *) arg, sizeof(dump_page_t)) != 0) {
			printk("SOO Agency: %s failed to get args from userspace!\n", __func__);
			return -EFAULT;
		}

		lprintk("%s: PID: %d\n", __func__, dump_page.pid);
		lprintk("%s: addr = %x\n", __func__, dump_page.addr);

		if (dump_page.pid == 0) {
			/* Ask the hypervisor to display the memory page */
			avz_dump_page(dump_page.addr >> 12);
		} else {

			for_each_process(p) {

				if (p->mm == NULL)
					continue;

				if (p->pid == dump_page.pid) {

					i1 = dump_page.addr >> 20;
					ptr = (unsigned int *) p->mm->pgd[0];

					lprintk(" pmd [%d] = %x\n", i1, ptr[i1]);
					pte = __va(pmd_val(ptr[i1]) & ~((1 << 10) - 1));

					i2 = (dump_page.addr & 0xfffff) >> 12; /* Take the 8 bits for 2nd index */
					lprintk("   pte [%x] = %x    -512: %x\n", i2, pte[i2], ((pte[i2-512])));

					phys_addr = pte[i2] & 0xfffff000;

					lprintk("   phys_addr: %x\n", phys_addr);

					dumpPage(phys_addr, 4096);
				}
			}
		}

		break;
	case AGENCY_IOCTL_GET_UPGRADE_IMG:
		rc = ioctl_get_upgrade_image(arg);
		break;

	case AGENCY_IOCTL_STORE_VERSIONS:
		rc = ioctl_store_versions(arg);
		break;
	
	case AGENCY_IOCTL_GET_ME_SNAPSHOT:
		ioctl_get_ME_snapshot(arg);
		break;
	
	case INJECTOR_IOCTL_CLEAN_ME:
		injector_clean_ME();
		break;

	case INJECTOR_IOCTL_RETRIEVE_ME:
		injector_retrieve_ME(arg);
		break;
	
	default:
		lprintk("%s: Unrecognized IOCTL: 0x%x\n", __func__, cmd);
		BUG();
		break;
	}

	return rc;
}

/**
 * This is the SOO Core mmap implementation. It is used to map the upgrade
 * image which is in the ME. 
 */
static int agency_upgrade_mmap(struct file *filp, struct vm_area_struct *vma) {
	unsigned long start, size;

	start = devaccess_get_upgrade_pfn();
	size = vma->vm_end - vma->vm_start;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot = phys_mem_access_prot(filp, vma->vm_pgoff, size, vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, start, size, vma->vm_page_prot)) {
		lprintk("%s: remap_pfn failed.\n", __func__);
		BUG();
	}

	return 0;
}

DECLARE_WAIT_QUEUE_HEAD(wq_prod);
DECLARE_WAIT_QUEUE_HEAD(wq_cons);

static ssize_t agency_read(struct file *fp, char *buff, size_t length, loff_t *ppos) {
	int maxbytes;
        int bytes_to_read;
        int bytes_read;
	void *ME;

	/* Wait for the Injector to produce data */
	wait_event_interruptible(wq_cons, injector_is_full() == true);
#if 0
	void *ME = injector_get_ME_buffer();
        maxbytes = injector_get_ME_size() - *ppos;
#else
	ME = injector_get_tmp_buf();
        maxbytes = injector_get_tmp_size();
#endif

        if (maxbytes > length)
                bytes_to_read = length;
        else
                bytes_to_read = maxbytes;
		
        bytes_read = copy_to_user(buff, ME, bytes_to_read);

	/* Notify the Injector we read the buffer */
	injector_set_full(false);
	wake_up_interruptible(&wq_prod);

        return bytes_read;
}

struct file_operations agency_fops = {
    .owner = THIS_MODULE,
    .open = agency_open,
    .read = agency_read,
    .release = agency_release,
    .unlocked_ioctl = agency_ioctl,
    .mmap = agency_upgrade_mmap,
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
    .uevent = soo_uevent,
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

/* Debugvar debugging facility */
extern ssize_t dbgvar_show(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t dbgvar_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
DEVICE_ATTR_RW(dbgvar);

/* Smart Object agencyUID management */
DEVICE_ATTR_RW(agencyUID);

/* Smart Object name management */
DEVICE_ATTR_RW(soo_name);

static struct attribute *soo_dev_attrs[] = {
    &dev_attr_dbgvar.attr,
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

#endif /* CONFIG_ARM */

int evtchn;

irqreturn_t dummy_interrupt(int irq, void *dev_id) {

	lprintk("### Got the interrupt %d on CPU: %d\n", irq, smp_processor_id());

	notify_remote_via_evtchn(evtchn);
	return IRQ_HANDLED;
};

static int agency_reboot_notify(struct notifier_block *nb, unsigned long code, void *unused)
{
	int slotID;
	ME_desc_t desc;

	lprintk("%s: !! Now terminating all Mobile Entities of this smart object...\n", __func__);

	for (slotID = 2; slotID < MAX_DOMAINS; slotID++) {

		/* Check if the ME slot is use dor not in order to not
		   call force_terminate on an empty slot, which would cause an error. */

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

	/* At this point, we can start the Discovery process */
	soolink_netsimul_init();

	/* This thread will start various debugging testings possibly in RT domain .*/
	kernel_thread(rtapp_main, NULL, 0);

	/* Leaving the thread in a sane way... */
	do_exit(0);

	return 0;
}

int agency_init(void) {
#ifdef CONFIG_ARM
	int rc;
#endif

	DBG("SOO Migration subsystem registering...\n");

	soo_sysfs_init();

	soo_guest_activity_init();

#ifdef CONFIG_ARM

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

	/* Initialize the injector subsystem */
	injector_init(&wq_prod, &wq_cons);

	/* Initialize the dbgvar facility */
	dbgvar_init();

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
