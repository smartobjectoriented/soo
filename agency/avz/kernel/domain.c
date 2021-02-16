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
#include <percpu.h>
#include <config.h>
#include <sched.h>
#include <serial.h>
#include <domain.h>
#include <console.h>
#include <errno.h>
#include <softirq.h>
#include <sched-if.h>
#include <memory.h>

#include <soo/soo.h>

#include <asm/processor.h>
#include <asm/vfp.h>

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
#if 0
DEFINE_PER_CPU(struct vcpu *, curr_vcpu);

struct cpu_info {
	struct domain *d;
	ulong saved_regs[2];
};

#endif

int current_domain_id(void)
{
	return current->domain_id;
}

#if 0
/*
 * Creation of new domain context associated to the agency or a Mobile Entity.
 *
 * @domid is the domain number
 * @partial tells if the domain creation remains partial, without the creation of the vcpu structure which may intervene in a second step
 */
struct domain *domain_create(domid_t domid, int cpu_id)
{
	struct domain *d;

	if ((d = alloc_domain_struct()) == NULL)
		return NULL;

	memset(d, 0, sizeof(*d));
	d->domain_id = domid;

	if (!is_idle_domain(d)) {
		d->is_paused_by_controller = 1;
		atomic_inc(&d->pause_count);

		if (evtchn_init(d) != 0)
			BUG();
	}

	d->shared_info = alloc_heap_page();
	BUG_ON(!d);

	clear_page(d->shared_info);

	/* Create a logbool hashtable associated to this domain */
	d->shared_info->logbool_ht = ht_create(LOGBOOL_HT_SIZE);
	if (d->shared_info->logbool_ht == NULL)
		BUG();

	/* Will be used during the context_switch (cf kernel/entry-armv.S */

	d->arch.guest_context.sys_regs.vdacr = domain_val(DOMAIN_USER, DOMAIN_MANAGER) | domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |
		domain_val(DOMAIN_IO, DOMAIN_MANAGER);

	d->arch.guest_context.sys_regs.vusp = 0x0; /* svc stack hypervisor at the beginning */

	d->arch.guest_context.event_callback = 0;
	d->arch.guest_context.domcall = 0;

	if (is_idle_domain(d)) {
		d->addrspace.pgtable_paddr = (CONFIG_RAM_BASE + TTB_L1_SYS_OFFSET);
		d->addrspace.pgtable_vaddr = (CONFIG_HYPERVISOR_VIRT_ADDR + TTB_L1_SYS_OFFSET);

		d->addrspace.ttbr0[cpu_id] = cpu_get_ttbr0() & ~TTBR0_BASE_ADDR_MASK;
		d->addrspace.ttbr0[cpu_id] |= d->addrspace.pgtable_paddr;
	}

	d->processor = cpu_id;

	spin_lock_init(&d->virq_lock);

	if (is_idle_domain(d))
	{
		d->runstate = RUNSTATE_running;
	}
	else
	{
		d->runstate = RUNSTATE_offline;
		set_bit(_VPF_down, &d->pause_flags);
	}

	/* Now, we assign a scheduling policy for this domain */

	if (is_idle_domain(d) && (cpu_id == AGENCY_CPU))
		d->sched = &sched_agency;
	else {

		if (cpu_id == ME_CPU) {

			d->sched = &sched_flip;
			d->need_periodic_timer = true;

		} else if (cpu_id == AGENCY_CPU) {

			d->sched = &sched_agency;
			d->need_periodic_timer = true;

		} else if (cpu_id == AGENCY_RT_CPU)

			d->sched = &sched_agency;

	}

	if (sched_init_domain(d, cpu_id) != 0)
		BUG();

	return d;
}


/* Complete domain destroy */
static void complete_domain_destroy(struct domain *d)
{
	sched_destroy_domain(d);

	/* Free the logbool hashtable associated to this domain */
	ht_destroy((logbool_hashtable_t *) d->shared_info->logbool_ht);

	/* Free start_info structure */

	free_heap_page((void *) d->vstartinfo_start);
	free_heap_page((void *) d->shared_info);
	free_heap_pages((void *) d->domain_stack, STACK_ORDER);

	free_domain_struct(d);
}

/* Release resources belonging to a domain */
void domain_destroy(struct domain *d)
{
	BUG_ON(!d->is_dying);

	complete_domain_destroy(d);
}

#endif

void vcpu_pause(struct domain *d)
{
	ASSERT(d != current);
	atomic_inc(&d->pause_count);
	vcpu_sleep_sync(d);
}

void vcpu_pause_nosync(struct domain *d)
{
	atomic_inc(&d->pause_count);
	vcpu_sleep_nosync(d);
}

void vcpu_unpause(struct domain *d)
{
	if (atomic_dec_and_test(&d->pause_count))
		vcpu_wake(d);
}

void domain_pause(struct domain *d)
{
	ASSERT(d != current);

	atomic_inc(&d->pause_count);

	vcpu_sleep_sync(d);
}

void domain_unpause(struct domain *d)
{
	if (atomic_dec_and_test(&d->pause_count))
		vcpu_wake(d);
}

void domain_pause_by_systemcontroller(struct domain *d) {
	/* We must ensure that the domain is not already paused */
	BUG_ON(d->is_paused_by_controller);

	if (!test_and_set_bool(d->is_paused_by_controller))
		domain_pause(d);
}

#if 0
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
	if (d != NULL)
		memset(d, 0, sizeof(*d));
	return d;
}
#endif

void context_switch(struct domain *prev, struct domain *next)
{
	local_irq_disable();

#if 0
	if (!is_idle_domain(current)) {

		prep_switch_domain();

		local_irq_disable();  /* Again, if the guest re-enables the IRQ */

		/* Save the VFP context */
		vfp_save_state(prev);
	}

	if (!is_idle_domain(next)) {

		/* Restore the VFP context of the next guest */
		vfp_restore_state(next);

	}

	get_current_addrspace(&prev->addrspace);
	switch_mm(next, &next->addrspace);

	/* Clear running flag /after/ writing context to memory. */
	smp_mb();

	prev->is_running = 0;

	/* Check for migration request /after/ clearing running flag. */
	smp_mb();

	spin_unlock(&prev->sched->sched_data.schedule_lock);

	switch_to(prev, next, prev);
#endif
}

#if 0

extern void ret_to_user(void);
extern void pre_ret_to_user(void);

/*
 * Initialize the domain stack used by the hypervisor.
 * This the H-stack and contains the reference to the VCPU in its base.
 */
void *setup_dom_stack(struct domain *d) {
	unsigned char *domain_stack;
	struct cpu_info *ci;

	domain_stack = alloc_heap_pages(STACK_ORDER, MEMF_bits(32));

	if (domain_stack == NULL)
	  return NULL;

	d->domain_stack = (unsigned long) domain_stack;

	ci = (struct cpu_info *) domain_stack;
	ci->d = d;

	/* Reserve the frame which will be restored later */
	domain_stack += STACK_SIZE - sizeof(struct cpu_user_regs);

	return domain_stack;
}

/*
 * Set up the first thread of a domain (associated to vcpu *v)
 */
void new_thread(struct domain *d, unsigned long start_pc, unsigned long fdt, unsigned long start_stack, unsigned long start_info)
{
	struct cpu_user_regs *domain_frame;
	struct cpu_user_regs *regs = &d->arch.guest_context.user_regs;

	domain_frame = (struct cpu_user_regs *) setup_dom_stack(d);

	if (domain_frame == NULL)
	  panic("Could not set up a new domain stack.n");

	domain_frame->r2 = fdt;
	domain_frame->r12 = start_info;

	domain_frame->r13 = start_stack;
	domain_frame->r15 = start_pc;

	domain_frame->psr = 0x93;  /* IRQs disabled initially */

	regs->r13 = (unsigned long) domain_frame;
	regs->r14 = (unsigned long) pre_ret_to_user;
}

static void continue_cpu_idle_loop(void)
{
	while (1) {
		local_irq_disable();

		raise_softirq(SCHEDULE_SOFTIRQ);
		do_softirq();

		ASSERT(local_irq_is_disabled());

		local_irq_enable();

		cpu_do_idle();
	}
}

void startup_cpu_idle_loop(void)
{

	ASSERT(is_idle_domain(current));

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
int domain_call(struct domain *target_dom, int cmd, void *arg)
{
	int rc;
	struct domain *__current;
	addrspace_t prev_addrspace;

	BUG_ON(local_irq_is_enabled());

	/* Switch the current domain to the target so that preserving ttbr0 during
	 * subsequent memory context switch will not affect the original one.
	 */

	__current = current;

	get_current_addrspace(&prev_addrspace);
	switch_mm(target_dom, &target_dom->addrspace);

	/* Make the call with IRQs disabled */

	rc = ((domcall_t) target_dom->arch.guest_context.domcall)(cmd, arg);

	/* Switch back to our domain address space. */
	switch_mm(__current, &prev_addrspace);

	return rc;
}

#endif
