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

#include <asm/domain.h>
#include <asm/vfp.h>

struct arch_domain {

	unsigned long vstartinfo_start;
	unsigned long domain_stack;

} __cacheline_aligned;


struct arch_vcpu {
	struct vcpu_guest_context guest_context;
	cpu_user_regs_t ctxt; /* User-level CPU registers */

	struct vfp_state vfp;

	uint32_t *guest_ptable;         /* physical address of L1 page table */
	uint32_t *guest_vtable;         /* virtual address of L1 page table */

	unsigned long	guest_pstart;		/* guest OS physical start address */
	unsigned long	guest_vstart;		/* guest OS virtual start address */


} __cacheline_aligned;


#define USE_NORMAL_PGTABLE	0
#define USE_SYSTEM_PGTABLE	1

extern void full_resume(void);

extern struct domain *agency_rt_domain;
extern struct domain *domains[MAX_DOMAINS];

extern int construct_agency(struct domain *d);
extern int construct_ME(struct domain *d);

extern struct vcpu *alloc_domU_vcpu0(struct domain *d);

extern void new_thread(struct vcpu *d, unsigned long start_pc, unsigned long r2_arg, unsigned long start_stack, unsigned long start_info);
void *setup_dom_stack(struct vcpu *v);

extern int domain_call(struct domain *target_dom, int cmd, void *arg, struct domain *current_mapped);
extern int prep_switch_domain(void);
extern void switch_mm(struct vcpu *prev, struct vcpu *next);
void machine_halt(void);

struct vcpu *alloc_vcpu(struct domain *d, unsigned int cpu_id);

struct vcpu *alloc_idle_vcpu(unsigned int cpu_id);
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

int arch_domain_create(struct domain *d);

void arch_domain_destroy(struct domain *d);

int domain_relinquish_resources(struct domain *d);

void dump_pageframe_info(struct domain *d);

void arch_dump_vcpu_info(struct vcpu *v);

void arch_dump_domain_info(struct domain *d);

void arch_vcpu_reset(struct vcpu *v);



#endif /* DOMAIN_H */
