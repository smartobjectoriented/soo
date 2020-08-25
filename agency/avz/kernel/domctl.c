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
#include <lib.h>
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

bool_t domctl_lock_acquire(void)
{

	/*
	 * Trylock here is paranoia if we have multiple privileged domains. Then
	 * we could have one domain trying to pause another which is spinning
	 * on domctl_lock -- results in deadlock.
	 */
	if (spin_trylock(&domctl_lock))
		return 1;

	return 0;
}

void domctl_lock_release(void)
{
	spin_unlock(&domctl_lock);
}

long do_domctl(domctl_t *args)
{
	long ret = 0;
	struct domctl curop, *op = &curop;

	memcpy(op, args, sizeof(domctl_t));

	if (!domctl_lock_acquire())
		return 0;

	switch ( op->cmd )
	{

	case DOMCTL_pauseME:
	{
		struct domain *d = domains[op->domain];

		ret = -ESRCH;
		if (d != NULL)
		{
			ret = -EINVAL;
			if (d != current->domain)
			{
				domain_pause_by_systemcontroller(d);
				ret = 0;
			}
		}
	}
	break;


	default:
		ret = arch_do_domctl(op, args);
		break;
	}

	domctl_lock_release();

	return ret;
}

long arch_do_domctl(struct domctl *op,  domctl_t *args)
{
    long ret = 0;
    struct start_info *si;

    switch (op->cmd)
    {
   
      case DOMCTL_unpauseME:
      {
        struct domain *d = domains[op->domain];
        ret = -ESRCH;

        if (d == NULL)
        	break;

        si = (struct start_info *) d->arch.vstartinfo_start;

        /* Retrieve info from hypercall parameter structure */
        si->store_mfn = op->u.unpause_ME.store_mfn;

        DBG("%s: unpausing ME\n", __FUNCTION__);

        domain_unpause_by_systemcontroller(d);

        ret = 0;
      }
      break;

    default:
        ret = -ENOSYS;
        break;
    }

    return ret;
}

