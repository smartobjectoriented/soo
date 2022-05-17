/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <memslot.h>
#include <smp.h>
#include <types.h>
#include <console.h>
#include <migration.h>
#include <domain.h>
#include <heap.h>

#include <device/irq.h>

#include <lib/crc.h>
#include <lib/image.h>

#include <libfdt/libfdt.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/logbool.h>

#include <soo/soo.h>

#include <soo_migration.h>
/*
 * Structures to store domain infos. Must be here and not locally in function,
 * since the maximum stack size is 8 KB
 */
static struct domain_migration_info dom_info = {0};

/* PFN offset of the target platform */
long pfn_offset = 0;

/*******************************************************************************
    MIGRATION INTERNALS
 *******************************************************************************/

/* Start of ME RAM in virtual address space of idle domain (extern) */
unsigned long vaddr_start_ME = 0;

/**
 * Initiate the migration process of a ME.
 */
void migration_init(soo_hyp_t *op) {
	unsigned int slotID = *((unsigned int *) op->p_val1);
	struct domain *domME = domains[slotID];

	DBG("Initializing migration of ME slotID=%d\n", slotID);

	switch (get_ME_state(slotID)) {

	case ME_state_suspended:
		DBG("ME state suspended\n");

		/* Initiator's side: the ME must be suspended during the migration */
		domain_pause_by_systemcontroller(domME);

		DBG0("ME paused OK\n");
		DBG("Being migrated: preparing to copy in ME_slotID %d: ME @ paddr 0x%08x (mapped @ vaddr 0x%08x in eventhypervisor)\n",
			slotID, (unsigned int) memslot[slotID].base_paddr, (unsigned int) vaddr_start_ME);

		break;

	case ME_state_booting:

		DBG("ME state booting\n");

		/* Initialize the ME descriptor */
		set_ME_state(slotID, ME_state_booting);

		/* Set the size of this ME in its own descriptor */
		domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;

		/* Now set the pfn base of this ME; this will be useful for the Agency Core subsystem */
		domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

		break;

	case ME_state_migrating:

		/* Target's side: nothing to do in particular */
		DBG("ME state migrating\n");

		/* Pre-init the basic information related to the ME */
		domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;
		domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

		break;

	default:
		printk("Agency: %s:%d Invalid state at this point (%d)\n", __func__, __LINE__, get_ME_state(slotID));
		BUG();
	}

	/* Used for future restore operation */
	vaddr_start_ME  = (unsigned long) __lva(memslot[slotID].base_paddr);
}

/*------------------------------------------------------------------------------
build_domain_migration_info
build_vcpu_migration_info
    Build the structures holding the key info to be migrated over
------------------------------------------------------------------------------*/
static void build_domain_migration_info(unsigned int ME_slotID, struct domain *me, struct domain_migration_info *mig_info)
{
	/* Domain ID */
	mig_info->domain_id = me->domain_id;

	/* Event channel info */

	memcpy(mig_info->evtchn, me->evtchn, sizeof(me->evtchn));

	/* Shared info */

	memcpy(mig_info->evtchn_pending, (bool *) &me->shared_info->evtchn_pending, sizeof(me->shared_info->evtchn_pending));

	/* Keep the a local clocksource reference */
	mig_info->clocksource_ref = me->shared_info->clocksource_ref;

	/* Get the start_info structure */
	memcpy(mig_info->start_info_page, (void *) me->si, PAGE_SIZE);

	/* ME_desc */
	memcpy(&mig_info->dom_desc, &me->shared_info->dom_desc, sizeof(dom_desc_t));

	/* Update the state for the ME instance which will migrate. The resident ME keeps its current state. */
	mig_info->dom_desc.u.ME.state = ME_state_migrating;

	/* Domain start pfn */

	mig_info->start_pfn = phys_to_pfn(memslot[ME_slotID].base_paddr);

	mig_info->pause_count = me->pause_count;

	mig_info->processor = me->processor;
	mig_info->need_periodic_timer = me->need_periodic_timer;

	/* Pause */
	mig_info->pause_flags = me->pause_flags;

	memcpy(&(mig_info->pause_count), &(me->pause_count), sizeof(me->pause_count));

	/* VIRQ mapping */
	memcpy(mig_info->virq_to_evtchn, me->virq_to_evtchn, sizeof((me->virq_to_evtchn)));

	/* Arch & address space */

	mig_info->cpu_regs = me->cpu_regs;
	mig_info->g_sp = me->g_sp;
	mig_info->vfp = me->vfp;

	mig_info->addrspace = me->addrspace;

	mig_info->evtchn_upcall_pending = me->shared_info->evtchn_upcall_pending;

	mig_info->version = me->shared_info->version;
	mig_info->tsc_timestamp = me->shared_info->tsc_timestamp;
	mig_info->tsc_prev = me->shared_info->tsc_prev;
}

/**
 * Read the migration info structures.
 */
void read_migration_structures(soo_hyp_t *op) {
	unsigned int ME_slotID = *((unsigned int *) op->p_val1);
	struct domain *domME = domains[ME_slotID];

	/* Gather all the info we need into structures */
	build_domain_migration_info(ME_slotID, domME, &dom_info);

	/* Copy structures to buffer */
	memcpy((void *) op->vaddr, &dom_info, sizeof(dom_info));

	/* Update op->size with valid data size */
	*((unsigned int *) op->p_val2) = sizeof(dom_info);
}

/*------------------------------------------------------------------------------
restore_domain_migration_info
restore_vcpu_migration_info
    Restore the migration info in the new ME structure
    Those function are actually exported and called in domain_migrate_restore.c
    They were kept in this file because they are the symmetric functions of
    build_domain_migration_info() and build_vcpu_migration_info()
------------------------------------------------------------------------------*/

static void restore_domain_migration_info(unsigned int ME_slotID, struct domain *me, struct domain_migration_info *mig_info)
{
	int i;

	DBG("%s\n", __func__);

	memcpy(me->evtchn, mig_info->evtchn, sizeof(me->evtchn));

	/*
	 * We reconfigure the inter-domain event channel so that we unbind the link to the previous
	 * remote domain (the agency in most cases), but we keep the state as it is since we do not
	 * want that the local event channel gets changed.
	 *
	 * Re-binding is performed during the resuming via vbus (backend side) OR
	 * if the ME gets killed, the event channel will be closed without any effect to a remote domain.
	 */

	for (i = 0; i < NR_EVTCHN; i++)
		if (me->evtchn[i].state == ECS_INTERDOMAIN)
			me->evtchn[i].interdomain.remote_dom = NULL;

	/* Shared info */
	memcpy((bool *) &me->shared_info->evtchn_pending, mig_info->evtchn_pending, sizeof((me->shared_info->evtchn_pending)));

	/* Retrieve the clocksource reference */
	me->shared_info->clocksource_ref = mig_info->clocksource_ref;

	me->tot_pages = memslot[ME_slotID].size >> PAGE_SHIFT;

	/* Restore start_info structure (allocated in the heap of hypervisor) */
	me->si = (struct start_info *) memalign(PAGE_SIZE, PAGE_SIZE);

	memcpy(me->si, mig_info->start_info_page, PAGE_SIZE);

	/* Restoring ME descriptor */
	memcpy(&me->shared_info->dom_desc, &mig_info->dom_desc, sizeof(dom_desc_t));

	/* Update the pfn of the ME in its host Smart Object */
	me->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[ME_slotID].base_paddr);

	/* start pfn can differ from the initiator according to the physical memory layout */
	me->si->dom_phys_offset = memslot[ME_slotID].base_paddr;
	me->si->nr_pages = me->tot_pages;

	pfn_offset = (me->si->dom_phys_offset >> PAGE_SHIFT) - mig_info->start_pfn;

	me->si->hypercall_vaddr = (unsigned long) hypercall_entry;

	me->si->domID = me->domain_id;

	me->si->printch = printch;

	me->pause_count = mig_info->pause_count;

	me->processor = mig_info->processor;

	me->need_periodic_timer = mig_info->need_periodic_timer;

	/* Pause */
	me->pause_flags = mig_info->pause_flags;

	memcpy(&(me->pause_count), &(mig_info->pause_count), sizeof(me->pause_count));

	/* VIRQ mapping */
	memcpy(me->virq_to_evtchn, mig_info->virq_to_evtchn, sizeof((me->virq_to_evtchn)));

	/* Fields related to CPU */
	me->cpu_regs = mig_info->cpu_regs;
	me->g_sp = mig_info->g_sp;
	me->vfp = mig_info->vfp;

	me->addrspace = mig_info->addrspace;

	/* Internal fields of vcpu_info_t structure */
	/* Must be the first field of this structure (see exception.S) */

	me->shared_info->evtchn_upcall_pending = mig_info->evtchn_upcall_pending;
	me->shared_info->version = mig_info->version;
	me->shared_info->tsc_timestamp = mig_info->tsc_timestamp;
	me->shared_info->tsc_prev = mig_info->tsc_prev;
}


/**
 * Write the migration info structures.
 */
void write_migration_structures(soo_hyp_t *op) {

	/* Get the migration info structures */
	memcpy(&dom_info, (void *) op->vaddr, sizeof(dom_info));
}

/**
 *  Inject a ME within a SOO device. This is the only possibility to load a ME within a Smart Object.
 *
 *  At the entry of this function, the ME ITB could have been allocated in the user space (via the injector application)
 *  or in the vmalloc'd area of the Linux kernel in case of a BT transfer from the tablet (using vuihandler).
 *
 *  To get rid of the way how the page tables are managed by Linux, we perform a copy of the ME ITB in the
 *  AVZ heap, assuming that the 8-MB heap is sufficient to host the ITB ME (< 2 MB in most cases).
 *
 *  If the ITB should become larger, it is still possible to compress (and enhance AVZ with a uncompressor invoked
 *  at loading time). Wouldn't be still not enough, a temporary fixmap mapping combined with get_free_pages should be envisaged
 *  to have the ME ITB accessible from the AVZ user space area.
 *
 * @param op  (op->vaddr is the ITB buffer, op->p_val1 will contain the slodID in return (-1 if no space), op->p_val2 is the ITB buffer size_
 */
void inject_me(soo_hyp_t *op)
{
	int slotID;
	size_t fdt_size;
	void *fdt_vaddr;
	int dom_size;
	struct domain *domME, *__current;
	addrspace_t prev_addrspace;
	unsigned long flags;
	void *itb_vaddr;
	size_t itb_size;

	DBG("%s: Preparing ME injection, source image = %lx\n", __func__, op->vaddr);

	flags = local_irq_save();

	/* op->vaddr: vaddr of itb */

	/* First, we do a copy of the ME ITB into the avz heap to get independent from Linux mapping (either
	 * in the user space, or in the vmalloc'd area
	 */
	itb_size = *((size_t *) op->p_val2);

	itb_vaddr = malloc(itb_size);
	BUG_ON(!itb_vaddr);

	memcpy(itb_vaddr, (void *) op->vaddr, itb_size);

	/* Retrieve the domain size of this ME through its device tree. */
	fit_image_get_data_and_size(itb_vaddr, fit_image_get_node(itb_vaddr, "fdt"), (const void **) &fdt_vaddr, &fdt_size);
	if (!fdt_vaddr) {
		printk("### %s: wrong device tree.\n", __func__);
		BUG();
	}

	dom_size = fdt_getprop_u32_default(fdt_vaddr, "/ME", "domain-size", 0);
	if (dom_size < 0) {
		printk("### %s: wrong domain-size prop/value.\n", __func__);
		BUG();
	}

	/* Find a slotID to store this ME. */
	slotID = get_ME_free_slot(dom_size, ME_state_booting);
	if (slotID == -1)
		goto out;

	domME = domains[slotID];

	/* Set the size of this ME in its own descriptor */
	domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;

	/* Now set the pfn base of this ME; this will be useful for the Agency Core subsystem */
	domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

	__current = current;

	get_current_addrspace(&prev_addrspace);

	switch_mm(idle_domain[smp_processor_id()], &idle_domain[smp_processor_id()]->addrspace);

	/* Clear the RAM allocated to this ME */
	memset((void *) __lva(memslot[slotID].base_paddr), 0, memslot[slotID].size);

	loadME(slotID, itb_vaddr);

	if (construct_ME(domains[slotID]) != 0)
		panic("Could not set up ME guest OS\n");

	/* Switch back to the agency address space */
	switch_mm(__current, &prev_addrspace);

out:
	/* Prepare to return the slotID to the caller. */
	*((unsigned int *) op->p_val1) = slotID;

	free(itb_vaddr);

	local_irq_restore(flags);
}

/*******************************************************************************
    EXPORTED FUNCTIONS
 *******************************************************************************/

void mig_restore_domain_migration_info(unsigned int ME_slotID, struct domain *me)
{
	return restore_domain_migration_info(ME_slotID, me, &dom_info);
}
