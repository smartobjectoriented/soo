/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef DOMAIN_H
#define DOMAIN_H

#include <soo/uapi/avz.h>

#include <asm/vfp.h>
#include <asm/mmu.h>

struct evtchn
{
	u8  state;             /* ECS_* */

	bool can_notify;

	struct {
		domid_t remote_domid;
	} unbound;     /* state == ECS_UNBOUND */

	struct {
		u16 remote_evtchn;
		struct domain *remote_dom;
	} interdomain; /* state == ECS_INTERDOMAIN */

	u16 virq;      /* state == ECS_VIRQ */

};

struct domain
{
	domid_t domain_id;

	/* Fields related to the underlying CPU */
	cpu_regs_t cpu_regs;
	addr_t   g_sp; 	/* G-stack */

	addr_t	event_callback;
	addr_t	domcall;

	struct vfp_state vfp;

	/* Information to the related address space for this domain. */
	addrspace_t addrspace;

	shared_info_t *shared_info;     /* shared data area */

	spinlock_t domain_lock;

	unsigned int tot_pages;       /* number of pages currently possesed */
	unsigned int max_pages;       /* maximum value for tot_pages        */

	/* Event channel information. */
	struct evtchn evtchn[NR_EVTCHN];
	spinlock_t event_lock;

	/* Is this guest dying (i.e., a zombie)? */
	enum { DOMDYING_alive, DOMDYING_dying, DOMDYING_dead } is_dying;

	/* Domain is paused by controller software? */
	bool_t is_paused_by_controller;

	int processor;

	bool need_periodic_timer;
	struct timer oneshot_timer;

	struct scheduler *sched;

	int runstate;

	/* Currently running on a CPU? */
	bool_t is_running;

	unsigned long pause_flags;
	atomic_t pause_count;

	/* IRQ-safe virq_lock protects against delivering VIRQ to stale evtchn. */
	u16 virq_to_evtchn[NR_VIRQS];
	spinlock_t virq_lock;

	unsigned long vstartinfo_start;
	unsigned long domain_stack;
};


#define USE_NORMAL_PGTABLE	0
#define USE_SYSTEM_PGTABLE	1

extern struct domain *agency_rt_domain;
extern struct domain *domains[MAX_DOMAINS];

extern int construct_agency(struct domain *d);
extern int construct_ME(struct domain *d);

extern void new_thread(struct domain *d, unsigned long start_pc, unsigned long r2_arg, unsigned long start_stack, unsigned long start_info);
void *setup_dom_stack(struct domain *d);

extern int domain_call(struct domain *target_dom, int cmd, void *arg);

void machine_halt(void);

void arch_domain_create(struct domain *d, int cpu_id);
void arch_setup_domain_frame(struct domain *d, cpu_regs_t *domain_frame, addr_t fdt_addr, addr_t start_info, addr_t start_stack, addr_t start_pc);

/*
 * setup_page_table_guestOS() is setting up the 1st-level and 2nd-level page tables within the domain.
 */
void __setup_dom_pgtable(struct domain *d, addr_t v_start, unsigned long map_size, addr_t p_start);

void vcpu_reset(struct vcpu *v);

/*
 * Arch-specifics.
 */

/* Allocate/free a domain structure. */
struct domain *alloc_domain_struct(void);
void free_domain_struct(struct domain *d);

/* Allocate/free a VCPU structure. */
struct vcpu *alloc_vcpu_struct(struct domain *d);

void free_vcpu_struct(struct vcpu *v);
void vcpu_destroy(struct vcpu *v);

void arch_domain_destroy(struct domain *d);

int domain_relinquish_resources(struct domain *d);

void dump_pageframe_info(struct domain *d);

void arch_dump_vcpu_info(struct vcpu *v);

void arch_dump_domain_info(struct domain *d);

void arch_vcpu_reset(struct vcpu *v);



#endif /* DOMAIN_H */
