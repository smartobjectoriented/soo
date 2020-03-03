/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <avz/config.h>
#include <avz/percpu.h>
#include <asm/linkage.h>

#include <avz/init.h>
#include <avz/sched.h>
#include <avz/lib.h>
#include <avz/console.h>
#include <avz/mm.h>
#include <avz/irq.h>

#include <asm/current.h>

#include <asm/cacheflush.h>
#include <asm/system.h>
#include <asm/string.h>
#include <asm/page.h>


/* The vectors page */
void *vectors_page;

/*
 * Pseudo-usr mode allows the hypervisor to switch back to the right stack (G-stach/H-stack) depending on whether
 * the guest issued a hypercall or if an interrupt occurred during some processing in the hypervisor.
 * 0 means we are in some hypervisor code, 1 means we are in some guest code.
 */
extern int pseudo_usr_mode[];

long do_set_callbacks(unsigned long event, unsigned long domcall)
{
	struct vcpu *v = (struct vcpu *) current;

	v->arch.guest_context.event_callback              = event;
	v->arch.guest_context.domcall                     = domcall;

	if (v->domain->domain_id == DOMID_AGENCY) {
		/*
		 * Do the same thing for the realtime subdomain.
		 */
		domains[DOMID_AGENCY_RT]->vcpu[0]->arch.guest_context.event_callback = v->arch.guest_context.event_callback;
		domains[DOMID_AGENCY_RT]->vcpu[0]->arch.guest_context.domcall = v->arch.guest_context.domcall;
	}

	return 0;
}

void dump_backtrace_entry(unsigned long where, unsigned long from)
{
	printk("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
}

long register_guest_nmi_callback(unsigned long address)
{
  /* not used */
    return 0;
}

long unregister_guest_nmi_callback(void)
{
  /* not used */
    return 0;
}

#if 0
void dumpregs(void) {
  unsigned int dacr, ttbcr, contextid, ttb0, ttbr1;

  __asm__ __volatile__("mrc p15, 0, %0, c3, c0, 0 " : "=r" (dacr) : : "memory", "cc");
  __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2 " : "=r" (ttbcr) : : "memory", "cc");
  __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 1 " : "=r" (contextid) : : "memory", "cc");
  __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0 " : "=r" (ttb0) : : "memory", "cc");
  __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 1 " : "=r" (ttbr1) : : "memory", "cc");

  printk("### DACR: %lx TTBCR: %lx ContextID: %lx TTB0: %lx TTBR1: %lx\n", dacr, ttbcr, contextid, ttb0, ttbr1);

}
#endif

void __init trap_init(void)
{
	extern char __stubs_start[], __stubs_end[];
	extern char __vectors_start[], __vectors_end[];

	memset(&pseudo_usr_mode, 0, NR_CPUS * sizeof(unsigned int));

	/*
	 * Copy the vectors, stubs and kuser helpers (in entry-armv.S)
	 * into the vector page, mapped at 0xffff0000, and ensure these
	 * are visible to the instruction stream.
	 */
	memcpy(vectors_page, __vectors_start, __vectors_end - __vectors_start);
	memcpy(vectors_page + 0x200, __stubs_start, __stubs_end - __stubs_start);

	flush_icache_range((unsigned long) vectors_page, (unsigned long) vectors_page + PAGE_SIZE);

}

extern void __backtrace(void);
void dump_stack(void)
{
	__backtrace();
}

asmlinkage void __div0(void)
{
	printk("Division by zero in kernel.\n");
	dump_stack();
}
EXPORT_SYMBOL(__div0);



