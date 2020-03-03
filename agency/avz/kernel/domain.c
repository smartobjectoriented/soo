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

#include <stdarg.h>
#include <avz/percpu.h>
#include <avz/config.h>
#include <avz/lib.h>
#include <avz/sched.h>
#include <avz/mm.h>
#include <avz/serial.h>
#include <avz/domain.h>
#include <avz/console.h>
#include <avz/errno.h>
#include <avz/mm.h>
#include <avz/softirq.h>
#include <avz/sched-if.h>

#include <soo/soo.h>

#include <asm/current.h>
#include <asm/system.h>
#include <mach/system.h>
#include <asm/processor.h>
#include <asm/vfp.h>
#include <asm/cpregs.h>
#include <asm/processor.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/logbool.h>

/*
 * We don't care of the IDLE domain here...
 * In the domain table, the index 0 and 1 are dedicated to the non-RT and RT agency domains.
 * The indexes 1..MAX_DOMAINS are for the MEs. ME_slotID should correspond to domain ID.
 */
struct domain *domains[MAX_DOMAINS];

struct domain *agency;

DEFINE_PER_CPU(struct vcpu *, curr_vcpu);

vcpu_info_t dummy_vcpu_info;

int current_domain_id(void)
{
	return current->domain->domain_id;
}

/*
 * Initialize the vcpu context associated to a domain accordin to its target cpu_id.
 */
struct vcpu *alloc_vcpu(struct domain *d, unsigned int cpu_id)
{
	struct vcpu *v;

	BUG_ON(!is_idle_domain(d) && d->vcpu[0]);

	if ((v = alloc_vcpu_struct(d)) == NULL)
		return NULL;

	v->domain = d;

	v->processor = cpu_id;

	spin_lock_init(&v->virq_lock);

	if (is_idle_domain(d))
	{
		v->runstate = RUNSTATE_running;
	}
	else
	{
		v->runstate = RUNSTATE_offline;
		set_bit(_VPF_down, &v->pause_flags);
		v->vcpu_info = (vcpu_info_t *) &shared_info(d, vcpu_info);
	}

	/* Now, we assign a scheduling policy for this domain */

	if (is_idle_domain(d) && (cpu_id == AGENCY_CPU))
		v->sched = &sched_agency;
	else {

		if (cpu_id == ME_STANDARD_CPU) {

			v->sched = &sched_flip;
			v->need_periodic_timer = true;

		} else if (cpu_id == ME_RT_CPU)

			v->sched = &sched_rt;

		else if (cpu_id == AGENCY_CPU) {

			v->sched = &sched_agency;
			v->need_periodic_timer = true;

		} else if (cpu_id == AGENCY_RT_CPU)

			v->sched = &sched_agency;

	}

	if (sched_init_vcpu(v, cpu_id) != 0)
	{
		free_vcpu_struct(v);
		return NULL;
	}

	d->vcpu[0] = v;

	return v;
}

/*
 * Finalize the domain creation by creating a new vcpu structure and related attributes.
 */
void finalize_domain_create(struct domain *d, bool realtime) {
	struct vcpu *vcpu;

	/* If the domain is subject to realtime constraints... */
	set_dom_realtime(d, realtime);

	/* Build up the vcpu structure */

	d->vcpu = xmalloc_array(struct vcpu *, 1);
	if (!d->vcpu)
		panic("xmalloc_array failed\n");

	memset(d->vcpu, 0, sizeof(*d->vcpu));

	if (is_idle_domain(d))
		vcpu = alloc_vcpu(d, smp_processor_id());
	else {

		if (d->domain_id == DOMID_AGENCY)
			vcpu = alloc_vcpu(d, AGENCY_CPU);
		else if (d->domain_id == DOMID_AGENCY_RT)
			vcpu = alloc_vcpu(d, AGENCY_RT_CPU);
		else
			vcpu = alloc_vcpu(d, (realtime ? ME_RT_CPU : ME_STANDARD_CPU));

	}

	if (vcpu == NULL)
		panic("alloc_vcpu failed\n");
}

/*
 * Creation of new domain context associated to the agency or a Mobile Entity.
 * @domid is the domain number
 * @realtime tells if the agency or the ME is realtime or not; this will affect the target CPU on which the domain will run.
 * @partial tells if the domain creation remains partial, without the creation of the vcpu structure which may intervene in a second step
 * in the case of an ME injection for example. In this case, @realtime is not used.
 */
struct domain *domain_create(domid_t domid, bool realtime, bool partial)
{
	struct domain *d;

	if ((d = alloc_domain_struct()) == NULL)
		return NULL;

	memset(d, 0, sizeof(*d));
	d->domain_id = domid;

	atomic_set(&d->refcnt, 1);

	if (!is_idle_domain(d)) {
		d->is_paused_by_controller = 1;
		atomic_inc(&d->pause_count);

		if (evtchn_init(d) != 0)
			goto fail;
	}

	if (arch_domain_create(d) != 0)
		goto fail;

	if (!partial)
		finalize_domain_create(d, realtime);

	return d;

	fail:
	d->is_dying = DOMDYING_dead;
	atomic_set(&d->refcnt, DOMAIN_DESTROYED);

	free_domain_struct(d);

	return NULL;
}


int domain_kill(struct domain *d)
{
	int rc = 0;

	if ( d == current->domain )
		return -EINVAL;

	/* Protected by domctl_lock. */
	switch ( d->is_dying )
	{
	case DOMDYING_alive:
		domain_pause(d);

		d->is_dying = DOMDYING_dying;

		spin_barrier(&d->domain_lock);

		evtchn_destroy(d);

		/* fallthrough */
	case DOMDYING_dying:

		d->is_dying = DOMDYING_dead;
		put_domain(d);


		/* fallthrough */
	case DOMDYING_dead:
		break;
	}

	return rc;
}

/* Complete domain destroy */
static void complete_domain_destroy(struct domain *d)
{
	struct vcpu *v;

	if ((v = d->vcpu[0]) != NULL)
		sched_destroy_vcpu(v);

	if ((v = d->vcpu[0]) != NULL) {
		free_vcpu_struct(v);
		xfree(d->vcpu);
	}

	/* Free the logbool hashtable associated to this domain */
	ht_destroy((logbool_hashtable_t *) d->shared_info->logbool_ht);

	/* Free start_info structure */

	free_heap_page((void *) d->arch.vstartinfo_start);
	free_heap_page((void *) d->shared_info);
	free_heap_pages((void *) d->arch.domain_stack, STACK_ORDER);

	free_domain_struct(d);
}

/* Release resources belonging to a domain */
void domain_destroy(struct domain *d)
{
	atomic_t old, new;

	BUG_ON(!d->is_dying);

	/* May be already destroyed, or get_domain() can race us. */
	_atomic_set(old, 0);
	_atomic_set(new, DOMAIN_DESTROYED);
	old = atomic_compareandswap(old, new, &d->refcnt);
	if ( _atomic_read(old) != 0 )
		return;

	complete_domain_destroy(d);
}

void vcpu_pause(struct vcpu *v)
{
	ASSERT(v != current);
	atomic_inc(&v->pause_count);
	vcpu_sleep_sync(v);
}

void vcpu_pause_nosync(struct vcpu *v)
{
	atomic_inc(&v->pause_count);
	vcpu_sleep_nosync(v);
}

void vcpu_unpause(struct vcpu *v)
{
	if (atomic_dec_and_test(&v->pause_count))
		vcpu_wake(v);
}

void domain_pause(struct domain *d)
{
	ASSERT(d != current->domain);

	atomic_inc(&d->pause_count);

	vcpu_sleep_sync(d->vcpu[0]);
}

void domain_unpause(struct domain *d)
{
	if (atomic_dec_and_test(&d->pause_count))
		vcpu_wake(d->vcpu[0]);
}

void domain_pause_by_systemcontroller(struct domain *d) {
	/* We must ensure that the domain is not already paused */
	BUG_ON(d->is_paused_by_controller);

	if (!test_and_set_bool(d->is_paused_by_controller))
		domain_pause(d);
}


void domain_unpause_by_systemcontroller(struct domain *d)
{
	if (test_and_clear_bool(d->is_paused_by_controller))
		domain_unpause(d);
}

void free_domain_struct(struct domain *d)
{
	free_heap_pages(d, get_order_from_bytes(sizeof(*d)));
}

struct domain *alloc_domain_struct(void)
{
	struct domain *d;
	/*
	 * We pack the PDX of the domain structure into a 32-bit field within
	 * the page_info structure. Hence the MEMF_bits() restriction.
	 */
	unsigned int bits = 32 + PAGE_SHIFT;

	d = alloc_heap_pages(get_order_from_bytes(sizeof(*d)), MEMF_bits(bits));
	if ( d != NULL )
		memset(d, 0, sizeof(*d));
	return d;
}

int arch_domain_create(struct domain *d)
{
	if ((d->shared_info = alloc_heap_page()) == NULL)
		BUG();

	clear_page(d->shared_info);

	/* Create a logbool hashtable associated to this domain */

	d->shared_info->logbool_ht = ht_create(LOGBOOL_HT_SIZE);

	if (d->shared_info->logbool_ht == NULL)
		BUG();

	return 0;
}


void setup_sys_regs_pgtable(struct vcpu *v) {

	v->arch.guest_context.sys_regs.guest_ttbr0 = pagetable_get_paddr(v->arch.guest_table);
	v->arch.guest_context.sys_regs.guest_ttbr0 |= TTB_FLAGS_SMP;

	v->arch.guest_context.sys_regs.guest_ttbr1 = pagetable_get_paddr(v->arch.guest_table);
	v->arch.guest_context.sys_regs.guest_ttbr1 |= TTB_FLAGS_SMP;
	v->arch.guest_context.sys_regs.guest_context_id = 0;
}


struct vcpu *alloc_vcpu_struct(struct domain *d)
{
	struct vcpu *v;

	v = alloc_heap_pages(get_order_from_bytes(sizeof(*v)), MEMF_bits(32));

	if (v != NULL)
		memset(v, 0, sizeof(*v));
	else
		return NULL;

	/* Will be used during the context_switch (cf kernel/entry-armv.S */

	v->arch.guest_context.sys_regs.vdacr = domain_val(DOMAIN_USER, DOMAIN_MANAGER) | domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |
			domain_val(DOMAIN_TABLE, DOMAIN_MANAGER) | domain_val(DOMAIN_IO, DOMAIN_CLIENT);

	v->arch.guest_context.sys_regs.vusp = 0x0; /* svc stack hypervisor at the beginning */

	v->arch.guest_context.event_callback = 0;
	v->arch.guest_context.domcall = 0;

	if (is_idle_domain(d)) {
		save_ptbase(v);
		setup_sys_regs_pgtable(v);
	}

	return v;
}

void free_vcpu_struct(struct vcpu *v)
{
	free_heap_page(v);
}

void context_switch(struct vcpu *prev, struct vcpu *next)
{
	local_irq_disable();

	if (!is_idle_domain(current->domain)) {

		prep_switch_domain();

		local_irq_disable();  /* Again, if the guest re-enables the IRQ */

		/* Save the VFP context */
		vfp_save_state(prev);
	}

	if (!is_idle_domain(next->domain)) {

		/* Restore the VFP context of the next guest */
		vfp_restore_state(next);

	}

	switch_mm(NULL, next);

	/* Clear running flag /after/ writing context to memory. */
	dmb();
	prev->is_running = 0;
	/* Check for migration request /after/ clearing running flag. */
	dmb();

	spin_unlock(&prev->sched->sched_data.schedule_lock);

	switch_to(prev, next, prev);

}

extern void ret_to_user(void);
extern void pre_ret_to_user(void);

/*
 * Initialize the domain stack used by the hypervisor.
 * This the H-stack and contains the reference to the VCPU in its base.
 */
void *setup_dom_stack(struct vcpu *v) {
	unsigned char *domain_stack;
	struct cpu_info *ci;

	domain_stack = alloc_heap_pages(STACK_ORDER, MEMF_bits(32));

	if (domain_stack == NULL)
	  return NULL;

	v->domain->arch.domain_stack = (unsigned long) domain_stack;

	ci = (struct cpu_info *) domain_stack;
	ci->cur_vcpu = v;

	/* Reserve the frame which will be restored later */
	domain_stack += STACK_SIZE - sizeof(struct cpu_user_regs);

	return domain_stack;
}

/*
 * Set up the first thread of a domain (associated to vcpu *v)
 */
void new_thread(struct vcpu *v, unsigned long start_pc, unsigned long fdt, unsigned long start_stack, unsigned long start_info)
{
	struct cpu_user_regs *domain_frame;
	struct cpu_user_regs *regs = &v->arch.guest_context.user_regs;

	domain_frame = (struct cpu_user_regs *) setup_dom_stack(v);

	if (domain_frame == NULL)
	  panic("Could not set up a new domain stack.n");

	domain_frame->r2 = fdt;
	domain_frame->r12 = start_info;

	domain_frame->r13 = start_stack;
	domain_frame->r15 = start_pc;

	domain_frame->psr = 0x93;  /* IRQs disabled initially */

	regs->r13 = (unsigned long) domain_frame;
	regs->r14 = (unsigned long) pre_ret_to_user;

	setup_sys_regs_pgtable(v);

}

static void continue_cpu_idle_loop(void)
{
	while (1) {
		local_irq_disable();

		raise_softirq(SCHEDULE_SOFTIRQ);
		do_softirq();

		ASSERT(local_irq_is_disabled());

		local_irq_enable();
		arch_idle();
	}
}

void startup_cpu_idle_loop(void)
{
	struct vcpu *v = current;

	ASSERT(is_idle_vcpu(v));

	cpu_set(smp_processor_id(), cpu_online_map);

	raise_softirq(SCHEDULE_SOFTIRQ);

	continue_cpu_idle_loop();
}

void machine_halt(void)
{
	printk("machine_halt called: spinning....\n");

	while (1);
}

void machine_restart(unsigned int delay_millisecs)
{
	printk("machine_restart called: spinning....\n");

	while (1);
}

/*
 * dommain_call
 *    Run a domain routine from hypervisor
 *    @target_dom is the domain which routine is executed
 *    @current_mapped is the domain which page table is currently loaded.
 *    @current_mapped_mode indicates if we consider the swapper pgdir or the normal page table (see switch_mm() for complete description)
 */
int domain_call(struct domain *target_dom, int cmd, void *arg, struct domain *current_mapped)
{
	int rc;

	ASSERT(current_mapped != NULL);
	BUG_ON(local_irq_is_enabled());

	/* Switch domain address space? */
	if (current_mapped != target_dom)
		switch_mm(current_mapped->vcpu[0], target_dom->vcpu[0]);

	/* Make the call with IRQs disabled */

	rc = ((domcall_t) target_dom->vcpu[0]->arch.guest_context.domcall)(cmd, arg);
	 
	/* Switch back domain address space? */
	if (current_mapped != target_dom)
		switch_mm(target_dom->vcpu[0], current_mapped->vcpu[0]);

	return rc;
}

