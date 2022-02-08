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

#ifndef OPENCN_SOO_H
#define OPENCN_SOO_H

#include <soo/uapi/avz.h>

/* Agency */
#define DOMID_CPU0		0

/* Realtime CPU domain */
#define DOMID_RT_CPU		1

typedef uint16_t domid_t;

struct dom_evtchn;

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

struct domain {
	domid_t          domain_id;

	spinlock_t       event_lock;
	struct evtchn    evtchn[NR_EVTCHN];
	int		 processor;

	shared_info_t   *shared_info;
};


long do_event_channel_op(int cmd, void *args);
asmlinkage void evtchn_do_upcall(struct pt_regs *regs);

void opencn_soo_init(void);

#endif /* OPENCN_SOO_H */
