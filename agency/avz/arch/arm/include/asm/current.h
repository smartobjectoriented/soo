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

#ifndef _ASMARM_CURRENT_H
#define _ASMARM_CURRENT_H

#include <soo/uapi/avz.h>

#include <avz/percpu.h>
#include <asm/page.h>

struct vcpu;
extern struct domain *agency;

DECLARE_PER_CPU(struct vcpu *, curr_vcpu);

struct cpu_info {
	struct vcpu *cur_vcpu;
	ulong saved_regs[2];
};

static inline struct cpu_info *current_cpu_info(void)
{
	register unsigned long sp asm("r13");
	return (struct cpu_info *) (sp & ~(STACK_SIZE - 1));
}

static inline struct vcpu *get_current(void)
{
  return current_cpu_info()->cur_vcpu;
}

#define current get_current()

static inline void set_current(struct vcpu *v)
{
    current_cpu_info()->cur_vcpu = v;
}

#define guest_cpu_user_regs()	(&current->arch.guest_context.user_regs)


/* XXX *#%(ing circular header dependencies force this to be a macro */
/* If the vcpu is running, its state is still on the stack, and the vcpu
 * structure's copy is obsolete. If the vcpu isn't running, the vcpu structure
 * holds the only copy. This routine always does the right thing. */
#define vcpu_regs(v) ({                 \
    struct cpu_user_regs *regs;         \
    if (v == current)                   \
        regs = guest_cpu_user_regs();   \
    else                                \
        regs = &v->arch.ctxt;           \
    regs;                               \
})

#endif /* _ASMARM_CURRENT_H */
