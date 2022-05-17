/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <config.h>
#include <types.h>
#include <memory.h>

#include <sched.h>
#include <domain.h>
#include <event.h>
#include <errno.h>

#include <asm/processor.h>

#include <device/irq.h>

#include <soo/uapi/domctl.h>
#include <soo/uapi/debug.h>

static DEFINE_SPINLOCK(domctl_lock);

extern long arch_do_domctl(struct domctl *op, domctl_t *args);

void do_domctl(domctl_t *args)
{
	struct domain *d;

	spin_lock(&domctl_lock);

	d = domains[args->domain];

	switch (args->cmd)
	{

	case DOMCTL_pauseME:

		domain_pause_by_systemcontroller(d);
		break;

	case DOMCTL_unpauseME:

		/* Retrieve info from hypercall parameter structure */
		d->si->store_mfn = args->u.unpause_ME.store_mfn;

		DBG("%s: unpausing ME\n", __func__);

		domain_unpause_by_systemcontroller(d);
		break;
	}

	spin_unlock(&domctl_lock);
}

