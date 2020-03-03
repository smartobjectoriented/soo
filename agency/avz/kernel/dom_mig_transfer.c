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

#include <fdt_support.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/memslot.h>
#include <asm/mach/map.h>

#include <avz/lib.h>
#include <avz/smp.h>

#include <avz/types.h>
#include <avz/console.h>
#include <avz/migration.h>
#include <avz/domain.h>
#include <avz/xmalloc.h>

#include <lib/crc.h>
#include <lib/image.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/logbool.h>

#include <soo/soo.h>

#include <soo_migration.h>
/*
 * Structures to store domain and vcpu infos. Must be here and not locally in function,
 * since the maximum stack size is 8 KB
 */
static struct domain_migration_info dom_info = {0};
static struct vcpu_migration_info vcpu_info = {0};

/* PFN offset of the target platform */
long pfn_offset = 0;

/*******************************************************************************
    MIGRATION INTERNALS
 *******************************************************************************/

/* Start of ME RAM in virtual address space of idle domain (extern) */
unsigned long vaddr_start_ME = 0;

/*------------------------------------------------------------------------------
switch_domain_address_space
------------------------------------------------------------------------------*/
void switch_domain_address_space(struct domain *from, struct domain *to)
{
	save_ptbase(from->vcpu[0]);
	write_ptbase(to->vcpu[0]);
}

/**
 * Initiate the migration process of a ME.
 */
int migration_init(soo_hyp_t *op) {
	unsigned int slotID = *((unsigned int *) op->p_val1);
	soo_personality_t pers = *((soo_personality_t *) op->p_val2);
	struct domain *domME = domains[slotID];

	DBG("slotID=%d, pers=%d\n", slotID, pers);

	switch (pers) {
	case SOO_PERSONALITY_INITIATOR:

		/* Initiator's side: the ME must be suspended during the migration */
		domain_pause_by_systemcontroller(domME);

		DBG0("ME paused OK\n");

		break;

	case SOO_PERSONALITY_SELFREFERENT:

		DBG("Self-referent\n");

		/* Create a domain context including the ME descriptor before the ME gets injected. */
		switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

		domME = domain_create(slotID, false, true);
		if (!domME)
			panic("Error creating the ME");

		domains[slotID] = domME;

		/* Initialize the ME descriptor */
		set_ME_state(slotID, ME_state_booting);

		/* Set the size of this ME in its own descriptor */
		domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;

		/* Now set the pfn base of this ME; this will be useful for the Agency Core subsystem */
		domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

		/* Switch back to dom0 address space */
		switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

		break;

	case SOO_PERSONALITY_TARGET:
		/* Target's side: nothing to do in particular */
		DBG("Target\n");

		/* Create the basic domain context including the ME descriptor (in its shared info page) */

		/* Switch to idle domain address space which has a full mapping of the RAM */
		switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

		/* Create new ME domain */
		domME = domain_create(slotID, is_me_realtime(), false);

		domains[slotID] = domME;

		if (domME == NULL) {
			printk("Error creating the ME\n");

			/* Switch back address space */
			switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);
			panic("Failure during domain creation ...\n");
		}

		/* Pre-init the basic information related to the ME */
		domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;
		domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

		/* Switch back to dom0 address space */
		switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

		break;
	}

	/* Used for future restore operation */
	vaddr_start_ME  = (unsigned long) __lva(memslot[slotID].base_paddr);

	if (pers == SOO_PERSONALITY_INITIATOR)
		DBG("Initiator: Preparing to copy in ME_slotID %d: ME @ paddr 0x%08x (mapped @ vaddr 0x%08x in hypervisor)\n", slotID, (unsigned int) memslot[slotID].base_paddr, (unsigned int) vaddr_start_ME);

	return 0;
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

	memcpy(mig_info->evtchn_pending, me->shared_info->evtchn_pending, sizeof(me->shared_info->evtchn_pending));

	/* Keep the a local clocksource reference */
	mig_info->clocksource_ref = me->shared_info->clocksource_ref;

	/* Get the start_info structure */
	memcpy(mig_info->start_info_page, (void *) me->arch.vstartinfo_start, PAGE_SIZE);

	/* ME_desc */
	memcpy(&mig_info->dom_desc, &me->shared_info->dom_desc, sizeof(dom_desc_t));

	/* Update the state for the ME instance which will migrate. The resident ME keeps its current state. */
	mig_info->dom_desc.u.ME.state = ME_state_migrating;

	/* Domain start pfn */
	mig_info->start_pfn = phys_to_pfn(memslot[ME_slotID].base_paddr);

	mig_info->pause_count = me->pause_count;

	dmb();

}

static void build_vcpu_migration_info(unsigned int ME_slotID, struct domain *me, struct vcpu_migration_info *mig_info)
{
	mig_info->processor = me->vcpu[0]->processor;
	mig_info->need_periodic_timer = me->vcpu[0]->need_periodic_timer;

	/* Pause */
	mig_info->pause_flags     = me->vcpu[0]->pause_flags;

	memcpy(&(mig_info->pause_count), &(me->vcpu[0]->pause_count), sizeof(me->vcpu[0]->pause_count));
	/* VIRQ mapping */
	memcpy(mig_info->virq_to_evtchn, me->vcpu[0]->virq_to_evtchn, sizeof((me->vcpu[0]->virq_to_evtchn)));

	/* Arch - including usr regs, vfp, etc. */
	memcpy(&(mig_info->arch), &(me->vcpu[0]->arch), sizeof(me->vcpu[0]->arch));

	/* Internal fields of vcpu_info_t structure */
	mig_info->evtchn_upcall_pending = me->vcpu[0]->vcpu_info->evtchn_upcall_pending;

	memcpy(&(mig_info->arch_info), &(me->vcpu[0]->vcpu_info->arch), sizeof(me->vcpu[0]->vcpu_info->arch));
	memcpy(&(mig_info->time), &(me->vcpu[0]->vcpu_info->time), sizeof(me->vcpu[0]->vcpu_info->time));

	dmb();
}

/**
 * Read the migration info structures.
 */
int read_migration_structures(soo_hyp_t *op) {
	unsigned int ME_slotID = *((unsigned int *) op->p_val1);
	struct domain *domME = domains[ME_slotID];
	unsigned long paddr;

	/* Gather all the info we need into structures */
	build_domain_migration_info(ME_slotID, domME, &dom_info);
	build_vcpu_migration_info(ME_slotID, domME, &vcpu_info);

	/* Switch to idle domain address space */
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

	/* Copy structures to buffer */
	paddr = op->paddr;

	memcpy((void *) __lva(paddr), &dom_info, sizeof(dom_info));
	paddr += sizeof(dom_info);
	memcpy((void *) __lva(paddr), &vcpu_info, sizeof(vcpu_info));
	paddr += sizeof(vcpu_info);

	dmb();

	/* Switch back to dom0 address space before updating size pointer! */
	switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

	/* Update op->size with valid data size */
	*((unsigned int *) op->p_val2) = sizeof(dom_info) + sizeof(vcpu_info);

	return 0;
}

/*
 * Get the info to know if the ME is realtime or not (used after migrating).
 */
bool is_me_realtime(void)
{
	return dom_info.dom_desc.realtime;
}


/*------------------------------------------------------------------------------
restore_domain_migration_info
restore_vcpu_migration_info
    Restore the migration info in the new ME structure
    Those function are actually exported and called in domain_migrate_restore.c
    They were kept in this file because they are the symmetric functions of
    build_domain_migration_info() and build_vcpu_migration_info()
------------------------------------------------------------------------------*/
extern char hypercall_start[];

static void restore_domain_migration_info(unsigned int ME_slotID, struct domain *me, struct domain_migration_info *mig_info)
{
	unsigned long vstartinfo_start;
	struct start_info *si;
	int i;

	DBG("%s\n", __FUNCTION__);

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
	memcpy(me->shared_info->evtchn_pending, mig_info->evtchn_pending, sizeof((me->shared_info->evtchn_pending)));

	/* Retrieve the clocksource reference */
	me->shared_info->clocksource_ref = mig_info->clocksource_ref;

	me->tot_pages = memslot[ME_slotID].size >> PAGE_SHIFT;

	/* Restore start_info structure (allocated in the heap of hypervisor) */
	vstartinfo_start = (unsigned long) alloc_heap_page();

	memcpy((struct start_info *) vstartinfo_start, mig_info->start_info_page, PAGE_SIZE);
	si = (start_info_t *) vstartinfo_start;

	/* Restoring ME descriptor */
	memcpy(&me->shared_info->dom_desc, &mig_info->dom_desc, sizeof(dom_desc_t));

	/* Update the pfn of the ME in its host Smart Object */
	me->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[ME_slotID].base_paddr);

	/* start pfn can differ from the initiator according to the physical memory layout */
	si->min_mfn = phys_to_pfn(memslot[ME_slotID].base_paddr);
	si->nr_pages = me->tot_pages;

	pfn_offset = si->min_mfn - mig_info->start_pfn;

	si->hypercall_addr = (unsigned long) hypercall_start;

	si->domID = me->domain_id;

	si->printch = printch;

	/* Re-init the startinfo_start address */
	me->arch.vstartinfo_start = vstartinfo_start;

	me->pause_count = mig_info->pause_count;

	dmb();
}

static void restore_vcpu_migration_info(unsigned int ME_slotID, struct domain *me, struct vcpu_migration_info *mig_info)
{
	DBG("%s\n", __FUNCTION__);

	me->vcpu[0]->processor = mig_info->processor;

	me->vcpu[0]->need_periodic_timer = mig_info->need_periodic_timer;

	/* Pause */
	me->vcpu[0]->pause_flags = mig_info->pause_flags;

	memcpy(&(me->vcpu[0]->pause_count), &(mig_info->pause_count), sizeof(me->vcpu[0]->pause_count));
	/* VIRQ mapping */
	memcpy(me->vcpu[0]->virq_to_evtchn, mig_info->virq_to_evtchn, sizeof((me->vcpu[0]->virq_to_evtchn)));

	/* Arch */
	memcpy(&(me->vcpu[0]->arch), &(mig_info->arch), sizeof(me->vcpu[0]->arch));

	/* Internal fields of vcpu_info_t structure */
	me->vcpu[0]->vcpu_info->evtchn_upcall_pending = mig_info->evtchn_upcall_pending;

	memcpy(&(me->vcpu[0]->vcpu_info->arch), &(mig_info->arch_info), sizeof(me->vcpu[0]->vcpu_info->arch));
	memcpy(&(me->vcpu[0]->vcpu_info->time), &(mig_info->time), sizeof(me->vcpu[0]->vcpu_info->time));

	dmb();
}

/**
 * Write the migration info structures.
 */
int write_migration_structures(soo_hyp_t *op) {
	unsigned long paddr;
	uint32_t crc32;

	crc32 = *((uint32_t *) op->p_val1);

	/* Switch to idle domain address space */
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

	/* Get the migration info structures */
	paddr = op->paddr;
	memcpy(&dom_info, (void *) __lva(paddr), sizeof(dom_info));

	dom_info.dom_desc.u.ME.crc32 = crc32;

	paddr += sizeof(dom_info);
	memcpy(&vcpu_info, (void *) __lva(paddr), sizeof(vcpu_info));

	dmb();

	/* Switch back to dom0 address space */
	switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

	return 0;
}

/*
 * Inject a ME within a SOO device. This is the only possibility to load a ME within a Smart Object.
 *
 * Returns 0 in case of success, -1 otherwise.
 */
int inject_me(soo_hyp_t *op)
{
	int rc = 0;
	unsigned int slotID;
	dtb_feat_t dtb_feat;
	size_t fdt_size;
	void *fdt_vaddr;
	int dom_size;
	struct domain *domME;

	DBG("%s: Preparing ME injection, source image = %lx\n", __func__, op->vaddr);

	/* op->vaddr: vaddr of itb */

	/* Retrieve the domain size of this ME through its device tree. */
	fit_image_get_data_and_size((void  *) op->vaddr, fit_image_get_node((void *) op->vaddr, "fdt"), (const void **) &fdt_vaddr, &fdt_size);
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
	slotID = get_ME_free_slot(dom_size);
	if (slotID < 1)
		goto out;

	/* Create a domain context including the ME descriptor before the ME gets injected. */

	domME = domain_create(slotID, false, true);
	if (!domME)
		panic("Error creating the ME");

	domains[slotID] = domME;

	/* Initialize the ME descriptor */
	set_ME_state(slotID, ME_state_booting);

	/* Set the size of this ME in its own descriptor */
	domME->shared_info->dom_desc.u.ME.size = memslot[slotID].size;

	/* Now set the pfn base of this ME; this will be useful for the Agency Core subsystem */
	domME->shared_info->dom_desc.u.ME.pfn = phys_to_pfn(memslot[slotID].base_paddr);

	/* Warning ! At the beginning of loadME(), a memory context switch is performed to access the AVZ system page table. */
	loadME(slotID, (unsigned char *) op->vaddr, &dtb_feat);

	DBG("ME realtime feature (0 = non-realtime, 1 = realtime): %d\n", dtb_feat.realtime);

	/* Finalize the domain creation. */
	finalize_domain_create(domains[slotID], dtb_feat.realtime);

	if (construct_ME(domains[slotID]) != 0)
		panic("Could not set up ME guest OS\n");

	domains[slotID]->shared_info->dom_desc.u.ME.crc32 = xcrc32((void *) __lva(memslot[slotID].base_paddr), memslot[slotID].size, 0xffffffff);

	/* Switch back to the agency address space */
	switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

out:
	/* Prepare to return the slotID to the caller. */
	*((unsigned int *) op->p_val1) = slotID;

	return rc;
}

/*******************************************************************************
    EXPORTED FUNCTIONS
 *******************************************************************************/

void mig_restore_domain_migration_info(unsigned int ME_slotID, struct domain *me)
{
	return restore_domain_migration_info(ME_slotID, me, &dom_info);
}

void mig_restore_vcpu_migration_info(unsigned int ME_slotID, struct domain *me)
{
	return restore_vcpu_migration_info(ME_slotID, me, &vcpu_info);
}
