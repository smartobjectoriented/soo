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

long do_domctl(long a1, long a2, long a3)
{
	struct domain *d;

	spin_lock(&domctl_lock);

	switch (a1)
	{

	case DOMCTL_get_AVZ_shared:
		return virt_to_phys(current->avz_shared);

	case DOMCTL_pauseME:

		d = domains[a2];

		domain_pause_by_systemcontroller(d);
		break;

	case DOMCTL_unpauseME:

		d = domains[a2];

		/* Retrieve info from hypercall parameter structure */
		d->avz_shared->vbstore_pfn = a3;

		DBG("%s: unpausing ME\n", __func__);

		domain_unpause_by_systemcontroller(d);
		break;
	}

	spin_unlock(&domctl_lock);

	return 0;
}

