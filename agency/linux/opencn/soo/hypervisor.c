/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/irqflags.h>
#include <linux/spinlock.h>

#include <linux/sched/debug.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include <opencn/opencn.h>

#include <opencn/soo/soo.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>

DEFINE_PER_CPU(struct domain, domain);

volatile shared_info_t *HYPERVISOR_shared_info;

int hypercall_trampoline(int hcall, long a0, long a1, long a2, long a3)
{
	unsigned long flags;
	struct pt_regs pt_regs;
	volatile shared_info_t *s = avz_shared_info;
	soo_hyp_t *soo_hyp;
	soo_hyp_dc_event_t *dc_event_args;
	struct domain *dom;

	/* Prepare to enter a pseudo-hypercall context.
	 * IRQs are disabled and a context frame is built up.
	 * It replaces the standard path in AVZ taking into account
	 * possible event channel processing in the upcall.
	 */

	pt_regs.flags = native_save_fl();

	local_irq_save(flags);

	switch (hcall) {

	case __HYPERVISOR_event_channel_op:

		do_event_channel_op(a0, (void *) a1);

		if (s->evtchn_upcall_pending)
			evtchn_do_upcall(&pt_regs);

		local_irq_restore(flags);
		break;

	case __HYPERVISOR_soo_hypercall:
		soo_hyp = (soo_hyp_t *) a0;

		switch (soo_hyp->cmd) {

		case AVZ_DC_SET:

			/*
			 * AVZ_DC_SET is used to assign a new dc_event number in the (target) domain shared info page.
			 * This has to be done atomically so that if there is still a "pending" value in the field,
			 * the hypercall must return with -BUSY; in this case, the caller has to busy-loop (using schedule preferably)
			 * until the field gets free, i.e. set to DC_NO_EVENT.
			 */
			dc_event_args = soo_hyp->p_val1;

			dom = &per_cpu(domain, dc_event_args->domID);
			BUG_ON(dom == NULL);

			/* The shared info page is set as non cacheable, i.e. if a CPU tries to update it, it becomes visible to other CPUs */
			if (atomic_cmpxchg(&dom->shared_info->dc_event, DC_NO_EVENT, dc_event_args->dc_event) != DC_NO_EVENT)
				return -EBUSY;
			break;

		default:
			lprintk("## %s: Unsupported SOO hypercall %d in OpenCN...\n", soo_hyp->cmd);
			BUG();

		}
		break;

	default:
		lprintk("## Unsupported hypercall %d in OpenCN...\n", hcall);
		BUG();
	}

	return 0;
}

/*
 * Initialization required to use SOO-related functions.
 * This is called from
 */
void opencn_soo_init(void) {

	/* Agency shared_info */
	memset(&per_cpu(domain, AGENCY_CPU0), 0, sizeof(struct domain));
	per_cpu(domain, AGENCY_CPU0).shared_info = kzalloc(sizeof(shared_info_t), GFP_KERNEL);
	spin_lock_init(&per_cpu(domain, AGENCY_CPU0).event_lock);


	/* RT Agency shared info */

	memset(&per_cpu(domain, AGENCY_RT_CPU), 0, sizeof(struct domain));
	per_cpu(domain, AGENCY_RT_CPU).shared_info = kzalloc(sizeof(shared_info_t), GFP_KERNEL);
	spin_lock_init(&per_cpu(domain, AGENCY_RT_CPU).event_lock);

	HYPERVISOR_shared_info = per_cpu(domain, AGENCY_CPU).shared_info;
	per_cpu(domain, AGENCY_CPU).shared_info->subdomain_shared_info = per_cpu(domain, AGENCY_RT_CPU).shared_info;

	/* Current domain/CPU topology */

	per_cpu(domain, AGENCY_CPU0).domain_id = DOMID_CPU0;
	per_cpu(domain, AGENCY_CPU0).processor = AGENCY_CPU0;

	per_cpu(domain, AGENCY_RT_CPU).domain_id = DOMID_RT_CPU;
	per_cpu(domain, AGENCY_RT_CPU).processor = AGENCY_RT_CPU;
}

postcore_initcall(opencn_soo_init)

