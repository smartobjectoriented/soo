/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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
#include <memory.h>
#include <sched.h>
#include <keyhandler.h>
#include <domain.h>
#include <errno.h>
#include <types.h>

#include <asm/io.h>
#include <asm/percpu.h>

#include <asm/cacheflush.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>

#include <soo_migration.h>

/**
 * Return the state of the ME corresponding to the ME_slotID.
 * If the ME does not exist anymore (for example, following a KILL_ME),
 * the state is set to ME_state_dead.
 */
ME_state_t get_ME_state(unsigned int ME_slotID) {
	if (domains[ME_slotID] == NULL)
		return ME_state_dead;
	else
		return domains[ME_slotID]->shared_info->dom_desc.u.ME.state;

}

void set_ME_state(unsigned int ME_slotID, ME_state_t state) {
	domains[ME_slotID]->shared_info->dom_desc.u.ME.state = state;
}

void shutdown_ME(unsigned int ME_slotID)
{
	struct domain *dom;
	struct domain *__current_domain;
	addrspace_t prev_addrspace;

	dom = domains[ME_slotID];

	/* Perform a removal of ME */
	dom->is_dying = DOMDYING_dead;
	DBG("Shutdowning slotID: %d - Domain pause nosync ...\n", ME_slotID);

	vcpu_pause(dom);

	DBG("Destroy evtchn if necessary - state: %d\n", get_ME_state(ME_slotID));
	evtchn_destroy(dom);

	DBG("Switching address space ...\n");

	__current_domain = current;
	get_current_addrspace(&prev_addrspace);

	switch_mm(idle_domain[smp_processor_id()], &idle_domain[smp_processor_id()]->addrspace);

	memset((void *) __lva(memslot[ME_slotID].base_paddr), 0, memslot[ME_slotID].size);

	switch_mm(__current_domain, &prev_addrspace);

	DBG("Destroying domain structure ...\n");

	domain_destroy(dom);

	DBG("-now resetting domains to NULL.\n");

	/* bye bye dear ME ! */
	domains[ME_slotID] = NULL;

	/* Reset the slot availability */
	put_ME_slot(ME_slotID);

}

/*
 * Check if some ME need to be killed.
 */
void check_killed_ME(void) {
	int i;

	for (i = MEMSLOT_BASE; i < MEMSLOT_NR; i++) {

		if (memslot[i].busy && (get_ME_state(i) == ME_state_killed))
			shutdown_ME(i);
	}
}

/***** SOO post-migration callbacks *****/

/*
 * Forward a domcall to the ME corresponding to <forward_slotID>
 *
 */
/*
 * agency_ctl()
 *
 * Request a specific action to the agency (issued from a ME for example)
 *
 */
int agency_ctl(agency_ctl_args_t *args)
{
	int rc = 0;
	struct domain *target_dom;
	soo_domcall_arg_t domcall_args;
	int cpu;

	memset(&domcall_args, 0, sizeof(soo_domcall_arg_t));

	/* Prepare the agency ctl args and the domcall args */
	memcpy(&domcall_args.u.agency_ctl_args, args, sizeof(agency_ctl_args_t));
	domcall_args.__agency_ctl = agency_ctl;

	/* Perform a domcall on the target ME */
	DBG("Processing agency_ctl function, cmd=0x%08x\n", args->cmd);

	switch (args->cmd) {

	case AG_KILL_ME:

		/* Performs a domcall in the ME to validate its removal. */

		domcall_args.cmd = CB_KILL_ME;
		target_dom = domains[args->slotID];

		/* Final shutdown of the ME if the state is set to killed will be performed elsewhere,
		 * by the caller.
		 */
		break;

	case AG_AGENCY_UID:
		memcpy(args->u.agencyUID_args.agencyUID.id, domains[0]->shared_info->dom_desc.u.agency.agencyUID.id, SOO_AGENCY_UID_SIZE);

		return rc;

	case AG_COOPERATE:

		domcall_args.cmd = CB_COOPERATE;

		/* This information must be provided by the initiator ME during the cooperation */

		memcpy(&domcall_args.u.cooperate_args.u.initiator_coop.pfn, &args->u.target_cooperate_args.pfn, sizeof(pfn_coop_t));

		/* Transfer the capabilities of the target ME */
		memcpy(domcall_args.u.cooperate_args.u.initiator_coop.spad_caps, domains[args->slotID]->shared_info->dom_desc.u.ME.spad.caps, SPAD_CAPS_SIZE);

		/* Transfer the SPID of the target ME */
		memcpy(domcall_args.u.cooperate_args.u.initiator_coop.spid, domains[args->slotID]->shared_info->dom_desc.u.ME.spid, SPID_SIZE);


		domcall_args.u.cooperate_args.role = COOPERATE_TARGET;
		target_dom = domains[args->slotID];

		break;

	default:

		domcall_args.cmd = CB_AGENCY_CTL;

		target_dom = domains[0]; /* Agency */

	}

	/*
	 * current_mapped_domain is associated to each CPU.
	 */
	cpu = smp_processor_id();

	/* Originating ME */
	domcall_args.slotID = current->domain_id;

	rc = domain_call(target_dom, DOMCALL_soo, &domcall_args);
	if (rc != 0) {
		printk("%s: DOMCALL_soo failed with rc = %d\n", __func__, rc);
		return rc;
	}

	DBG("Ending forward callback now, back to the originater...\n");

	/* Copy the agency ctl args back */
	memcpy(args, &domcall_args.u.agency_ctl_args, sizeof(agency_ctl_args_t));

	return rc;
}

/**
 * Initiate the pre-propagate callback on a ME.
 */
static int soo_pre_propagate(unsigned int slotID, int *propagate_status) {
	soo_domcall_arg_t domcall_args;
	int rc;

	memset(&domcall_args, 0, sizeof(domcall_args));

	domcall_args.cmd = CB_PRE_PROPAGATE;
	domcall_args.__agency_ctl = agency_ctl;

	DBG("Pre-propagate callback being issued...\n");

	domcall_args.slotID = slotID;

	if ((rc = domain_call(domains[slotID], DOMCALL_soo, &domcall_args)) != 0) {
		printk("%s: DOMCALL failed (%d)\n", __func__, rc);
		BUG();
	}

	*propagate_status = domcall_args.u.pre_propagate_args.propagate_status;

	/* If the ME decides itself to be removed */
	check_killed_ME();

	return 0;
}

int soo_pre_activate(unsigned int slotID)
{
	soo_domcall_arg_t domcall_args;
	int rc;

	memset(&domcall_args, 0, sizeof(domcall_args));

	domcall_args.cmd = CB_PRE_ACTIVATE;
	domcall_args.__agency_ctl = agency_ctl;

	domcall_args.slotID = slotID;

	/* Perform a domcall on the specific ME */
	DBG("Pre-activate callback being issued...\n");

	rc = domain_call(domains[slotID], DOMCALL_soo, &domcall_args);
	if (rc != 0) {
		printk("%s: DOMCALL_soo failed with rc = %d\n", __func__, rc);
		return rc;
	}

	check_killed_ME();

	return 0;
}

/*
 * soo_cooperate()
 *
 * Perform a cooperate callback in the target (incoming) ME with a set of target ready-to-cooperate MEs.
 *
 */

int soo_cooperate(unsigned int slotID)
{
	soo_domcall_arg_t domcall_args;
	int i, rc, avail_ME;
	bool itself;   /* Used to detect a ME of a same SPID */

	/* Are we OK to collaborate ? */
	if (!domains[slotID]->shared_info->dom_desc.u.ME.spad.valid)
		return false;

	/* Reset anything in the cooperate_args */
	memset(&domcall_args, 0, sizeof(domcall_args));

	domcall_args.cmd = CB_COOPERATE;
	domcall_args.__agency_ctl = agency_ctl;

	domcall_args.u.cooperate_args.role = COOPERATE_INITIATOR;

	avail_ME = 0;

	domcall_args.u.cooperate_args.alone = true;

	for (i = MEMSLOT_BASE; i < MEMSLOT_NR; i++) {

		if ((i != slotID) && memslot[i].busy) {
			domcall_args.u.cooperate_args.alone = false;

			/*
			 * The cooperation
			 */

			/*
			 * Check if this residing ME has the same SPID than the initiator ME (arrived ME). If the residing ME is not ready
			 * to cooperate, the spid is passed as argument anyway so that the arrived ME can be aware of its presence and decide
			 * to do something accordingly.
			 */
			itself = !memcmp(domains[i]->shared_info->dom_desc.u.ME.spid, domains[slotID]->shared_info->dom_desc.u.ME.spid, SPID_SIZE);

			/* If the ME authorizes us to enter into a cooperation process... */
			if (domains[i]->shared_info->dom_desc.u.ME.spad.valid || itself) {

				domcall_args.u.cooperate_args.u.target_coop_slot[avail_ME].slotID = i;
				domcall_args.u.cooperate_args.u.target_coop_slot[avail_ME].spad.valid = domains[i]->shared_info->dom_desc.u.ME.spad.valid;

				/* Transfer the capabilities of the target ME */
				memcpy(domcall_args.u.cooperate_args.u.target_coop_slot[avail_ME].spad.caps, domains[i]->shared_info->dom_desc.u.ME.spad.caps, SPAD_CAPS_SIZE);

				/* Transfer the SPID of the target ME */
				memcpy(domcall_args.u.cooperate_args.u.target_coop_slot[avail_ME].spid, domains[i]->shared_info->dom_desc.u.ME.spid, SPID_SIZE);

				avail_ME++;
			}


		}
	}

	/* Well, at this point, we may have zero, one or several MEs for a possible cooperation.
	 * If there is no ME, we call the cooperate callback anyway to let the ME
	 * decide what to do (it has to detect ifself that no ME is ready to cooperate).
	 */

	/* Perform a domcall on the specific ME */
	DBG("Cooperate callback being issued...\n");

	domcall_args.slotID = slotID;

	rc = domain_call(domains[slotID], DOMCALL_soo, &domcall_args);
	if (rc != 0) {
		printk("%s: DOMCALL_soo failed with rc = %d\n", __func__, rc);
		return rc;
	}

	check_killed_ME();

	return 0;
}

static void dump_backtrace(unsigned char key)
{
	soo_domcall_arg_t domcall_args;
	unsigned long flags;

	printk("'%c' pressed -> dumping backtrace in guest\n", key);

	memset(&domcall_args, 0, sizeof(domcall_args));

	domcall_args.cmd = CB_DUMP_BACKTRACE;

	local_irq_save(flags);

	printk("Agency:\n\n");

	domain_call(domains[0], DOMCALL_soo, &domcall_args);

	printk("ME (dom 1):\n\n");

	domain_call(domains[1], DOMCALL_soo, &domcall_args);

	local_irq_restore(flags);
}

static void dump_vbstore(unsigned char key)
{
	soo_domcall_arg_t domcall_args;
	unsigned long flags;

	printk("'%c' pressed -> dumping vbstore (agency) ...\n", key);

	memset(&domcall_args, 0, sizeof(domcall_args));

	domcall_args.cmd = CB_DUMP_VBSTORE;

	local_irq_save(flags);

	domain_call(domains[0], DOMCALL_soo, &domcall_args);

	local_irq_restore(flags);
}


static struct keyhandler dump_backtrace_keyhandler = {
		.fn = dump_backtrace,
		.desc = "dump backtrace"
};

static struct keyhandler dump_vbstore_keyhandler = {
		.fn = dump_vbstore,
		.desc = "dump vbstore"
};

/**
 * Return the descriptor of a domain (agency or ME).
 * A size of 0 means there is no ME in the slot.
 */
void get_dom_desc(unsigned int slotID, dom_desc_t *dom_desc) {
	/* Check for authorization... (to be done) */

	/*
	 * If no ME is present in the slot specified by slotID, we assign a size of 0 in the ME descriptor.
	 * We presume that the slotID of agency is never free...
	 */
	if ((slotID > 1) && !memslot[slotID].busy)
		dom_desc->u.ME.size = 0;
	else
		/* Copy the content to the target desc */
		memcpy(dom_desc, &domains[slotID]->shared_info->dom_desc, sizeof(dom_desc_t));
}

/**
 * SOO hypercall processing.
 */
int do_soo_hypercall(soo_hyp_t *args) {
	int rc = 0;
	soo_hyp_t op;
	struct domain *dom;
	soo_hyp_dc_event_t *dc_event_args;
	soo_domcall_arg_t domcall_args;
	uint32_t slotID;

	memset(&domcall_args, 0, sizeof(soo_domcall_arg_t));

	/* Get argument from guest */
	memcpy(&op, args, sizeof(soo_hyp_t));

	/*
	 * Execute the hypercall
	 * The usage of args and returns depend on the hypercall itself.
	 * This has to be aligned with the guest which performs the hypercall.
	 */

	switch (op.cmd) {
	case AVZ_MIG_PRE_PROPAGATE:
		rc = soo_pre_propagate(*((unsigned int *) op.p_val1), op.p_val2);
		break;

	case AVZ_MIG_PRE_ACTIVATE:
		rc = soo_pre_activate(*((unsigned int *) op.p_val1));
		break;

#ifdef CONFIG_ARM32
	case AVZ_MIG_INIT:
		rc = migration_init(&op);
		break;

	case AVZ_MIG_FINAL:
		rc = migration_final(&op);
		break;

	case AVZ_GET_ME_FREE_SLOT:
	{
		unsigned int ME_size = *((unsigned int *) op.p_val1);
		int slotID;

		/*
		 * Try to get an available slot for a ME with this size.
		 * It will return -1 if no slot is available.
		 */
		slotID = get_ME_free_slot(ME_size);

		*((int *) op.p_val1) = slotID;

		break;
	}

	case AVZ_GET_DOM_DESC:
		get_dom_desc(*((unsigned int *) op.p_val1), (dom_desc_t *) op.p_val2);
		break;

	case AVZ_MIG_READ_MIGRATION_STRUCT:
		rc = read_migration_structures(&op);
		break;

	case AVZ_MIG_WRITE_MIGRATION_STRUCT:
		rc = write_migration_structures(&op);
		break;

	case AVZ_INJECT_ME:
		rc = inject_me(&op);
		break;

#endif /* CONFIG_ARM32 */

	case AVZ_DC_SET:
		/*
		 * AVZ_DC_SET is used to assign a new dc_event number in the (target) domain shared info page.
		 * This has to be done atomically so that if there is still a "pending" value in the field,
		 * the hypercall must return with -BUSY; in this case, the caller has to busy-loop (using schedule preferably)
		 * until the field gets free, i.e. set to DC_NO_EVENT.
		 */
		dc_event_args = (soo_hyp_dc_event_t *) op.p_val1;

		dom = domains[dc_event_args->domID];
		BUG_ON(dom == NULL);

		/* The shared info page is set as non cacheable, i.e. if a CPU tries to update it, it becomes visible to other CPUs */
		if (atomic_cmpxchg(&dom->shared_info->dc_event, DC_NO_EVENT, dc_event_args->dc_event) != DC_NO_EVENT)
			rc = -EBUSY;

		break;

	case AVZ_KILL_ME:

		slotID = *((unsigned int *) op.p_val1);
		shutdown_ME(slotID);

		break;

	case AVZ_GET_ME_STATE:

		*((unsigned int *) op.p_val1) = get_ME_state(*((unsigned int *) op.p_val1));

		break;

	case AVZ_SET_ME_STATE:
	{
		unsigned int *state = (unsigned int *) op.p_val1;

		set_ME_state(state[0], state[1]);

		break;
	}

	case AVZ_AGENCY_CTL:
		/*
		 * Primary agency ctl processing- The args contains the slotID of the ME the agency_ctl is issued from.
		 */

		agency_ctl((agency_ctl_args_t *) op.p_val1);

		break;

	default:
		printk("%s: Unrecognized hypercall: %d\n", __func__, op.cmd);
		BUG();
		break;
	}

	/* If all OK, copy updated structure to guest */
	memcpy(args, &op, sizeof(soo_hyp_t));

	flush_dcache_all();

	return rc;
}

int soo_activity_init(void) {

	DBG("Setting SOO avz up ...\n");

	register_keyhandler('b', &dump_backtrace_keyhandler);
	register_keyhandler('v', &dump_vbstore_keyhandler);

	return 0;
}

