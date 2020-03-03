/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
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


#ifndef __SCHED_H__
#define __SCHED_H__

#include <avz/config.h>
#include <avz/types.h>
#include <avz/spinlock.h>
#include <avz/smp.h>
#include <avz/shared.h>
#include <avz/time.h>
#include <avz/timer.h>
#include <avz/irq.h>
#include <avz/mm.h>

#include <soo/uapi/event_channel.h>

#include <soo/uapi/avz.h>

#include <public/vcpu.h>

#include <asm/domain.h>

/* A global pointer to the initial domain (Agency). */
extern struct domain *agency;
extern struct domain *domains[];

struct evtchn
{
	u8  state;             /* ECS_* */

	bool can_notify;

	struct {
		domid_t remote_domid;
	} unbound;     /* state == ECS_UNBOUND */

	struct {
		u16            remote_evtchn;
		struct domain *remote_dom;
	} interdomain; /* state == ECS_INTERDOMAIN */

	u16 virq;      /* state == ECS_VIRQ */

};

int  evtchn_init(struct domain *d); /* from domain_create */
void evtchn_destroy(struct domain *d); /* from domain_kill */
void evtchn_destroy_final(struct domain *d); /* from complete_domain_destroy */

struct vcpu 
{
    int              processor;

    vcpu_info_t     *vcpu_info;

    struct domain   *domain;

    bool    	     need_periodic_timer;
    struct timer     oneshot_timer;

    struct scheduler *sched;

    int      	     runstate;

    /* Currently running on a CPU? */
    bool_t           is_running;

    unsigned long    pause_flags;
    atomic_t         pause_count;

    /* IRQ-safe virq_lock protects against delivering VIRQ to stale evtchn. */
    u16              virq_to_evtchn[NR_VIRQS];
    spinlock_t       virq_lock;

    struct arch_vcpu arch;
};

/* Per-domain lock can be recursively acquired in fault handlers. */
#define domain_lock(d) spin_lock_recursive(&(d)->domain_lock)
#define domain_unlock(d) spin_unlock_recursive(&(d)->domain_lock)
#define domain_is_locked(d) spin_is_locked(&(d)->domain_lock)

struct domain
{
    domid_t          domain_id;

    shared_info_t   *shared_info;     /* shared data area */

    spinlock_t       domain_lock;

    unsigned int     tot_pages;       /* number of pages currently possesed */
    unsigned int     max_pages;       /* maximum value for tot_pages        */


    /* Event channel information. */
    struct evtchn    evtchn[NR_EVTCHN];
    spinlock_t       event_lock;

    /* Is this guest dying (i.e., a zombie)? */
    enum { DOMDYING_alive, DOMDYING_dying, DOMDYING_dead } is_dying;

    /* Domain is paused by controller software? */
    bool_t           is_paused_by_controller;

    atomic_t         pause_count;

    atomic_t         refcnt;

    struct vcpu    **vcpu;

    struct arch_domain arch;
};

extern struct vcpu *idle_vcpu[NR_CPUS];
#define is_idle_domain(d) ((d)->domain_id == DOMID_IDLE)
#define is_idle_vcpu(v)   (is_idle_domain((v)->domain))

#define DOMAIN_DESTROYED (1<<31) /* assumes atomic_t is >= 32 bits */
#define put_domain(_d) \
  if (atomic_dec_and_test(&(_d)->refcnt)) domain_destroy(_d)

/*
 * Use this when you don't have an existing reference to @d. It returns
 * FALSE if @d is being destroyed.
 */
static always_inline int get_domain(struct domain *d)
{
    atomic_t old, new, seen = d->refcnt;
    do
    {
        old = seen;
        if (unlikely(_atomic_read(old) & DOMAIN_DESTROYED))
            return 0;
        _atomic_set(new, _atomic_read(old) + 1);
        seen = atomic_compareandswap(old, new, &d->refcnt);
    }
    while (unlikely(_atomic_read(seen) != _atomic_read(old)));
    return 1;
}

/*
 * Creation of new domain context associated to the agency or a Mobile Entity.
 * @domid is the domain number
 * @realtime tells if the agency or the ME is realtime or not; this will affect the target CPU on which the domain will run.
 * @partial tells if the domain creation remains partial, without the creation of the vcpu structure which may intervene in a second step
 * in the case of an ME injection for example. In this case, @realtime is not used.
 */
struct domain *domain_create(domid_t domid, bool realtime, bool partial);
void finalize_domain_create(struct domain *d, bool realtime);

void domain_destroy(struct domain *d);
int domain_kill(struct domain *d);
void domain_shutdown(struct domain *d, u8 reason);
void domain_resume(struct domain *d);

/*
 * Mark specified domain as crashed. This function always returns, even if the
 * caller is the specified domain. The domain is not synchronously descheduled
 * from any processor.
 */
void __domain_crash(struct domain *d);
#define domain_crash(d) do {                                              \
    printk("domain_crash called from %s:%d\n", __FILE__, __LINE__);       \
    __domain_crash(d);                                                    \
} while (0)

extern void vcpu_periodic_timer_work(struct vcpu *);

#define set_current_state(_s) do { current->state = (_s); } while (0)
void scheduler_init(void);

int  sched_init_vcpu(struct vcpu *v, unsigned int processor);
void sched_destroy_vcpu(struct vcpu *v);

void vcpu_wake(struct vcpu *d);
void vcpu_sleep_nosync(struct vcpu *d);
void vcpu_sleep_sync(struct vcpu *d);


/*
 * Called by the scheduler to switch to another VCPU. This function must
 * call context_saved(@prev) when the local CPU is no longer running in
 * @prev's context, and that context is saved to memory. Alternatively, if
 * implementing lazy context switching, it suffices to ensure that invoking
 * sync_vcpu_execstate() will switch and commit @prev's state.
 */
void context_switch(struct vcpu *prev, struct vcpu *next);

/*
 * As described above, context_switch() must call this function when the
 * local CPU is no longer running in @prev's context, and @prev's context is
 * saved to memory. Alternatively, if implementing lazy context switching,
 * ensure that invoking sync_vcpu_execstate() will switch and commit @prev.
 */
void context_saved(struct vcpu *prev);

void startup_cpu_idle_loop(void);

/*
 * Per-VCPU pause flags.
 */
 /* Domain is blocked waiting for an event. */
#define _VPF_blocked         0
#define VPF_blocked          (1UL<<_VPF_blocked)

/* VCPU is offline. */
#define _VPF_down            1
#define VPF_down             (1UL<<_VPF_down)

void vcpu_unblock(struct vcpu *v);
void vcpu_pause(struct vcpu *v);
void vcpu_pause_nosync(struct vcpu *v);
void domain_pause(struct domain *d);
void vcpu_unpause(struct vcpu *v);
void domain_unpause(struct domain *d);
void domain_pause_by_systemcontroller(struct domain *d);
void domain_unpause_by_systemcontroller(struct domain *d);
void cpu_init(void);

int rt_quant_runnable(void);
struct task_slice rt_do_schedule(void);
struct task_slice flip_do_schedule(void);

#endif /* __SCHED_H__ */

