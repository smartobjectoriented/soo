/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/version.h>
#include <linux/cpumask.h>
#include <linux/mm_types.h>

#include <asm/pgtable.h>

#include <soo/hypervisor.h>
#include <soo/evtchn.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/schedop.h>
#include <soo/uapi/domctl.h>
#include <soo/uapi/physdev.h>

void avz_ME_unpause(domid_t domain_id, uint32_t store_mfn)
{
	struct domctl op;
	int ret;

	lprintk("Trying to unpause ME domain %d...", domain_id);

	op.cmd = DOMCTL_unpauseME;

	op.domain = domain_id;

	op.u.unpause_ME.store_mfn = store_mfn;

	ret = hypercall_trampoline(__HYPERVISOR_domctl, (long) &op, 0 ,0 ,0);

	if (ret == -ESRCH) {
		lprintk("no further ME !\n");
		return ;
	}
	else
		lprintk("done.\n");

	BUG_ON(ret< 0);
}

void avz_ME_pause(domid_t domain_id)
{
	struct domctl op;
	int ret;

	printk("Trying to pause domain %d...", domain_id);

	op.cmd = DOMCTL_pauseME;
	op.domain = domain_id;

	ret = hypercall_trampoline(__HYPERVISOR_domctl, (long) &op, 0, 0, 0);

	if (ret == -ESRCH) {
		printk("no further ME !\n");
		return ;
	} else
		printk("done.\n");

	BUG_ON(ret< 0);
}

int avz_dump_page(unsigned int pfn)
{
	int ret;

	ret = hypercall_trampoline(__HYPERVISOR_physdev_op, PHYSDEVOP_dump_page, (long) &pfn, 0, 0);
	BUG_ON(ret < 0);

	return 0;
}

int avz_dump_logbool(void)
{
	int ret;

	ret = hypercall_trampoline(__HYPERVISOR_physdev_op, PHYSDEVOP_dump_logbool, 0 ,0, 0);
	BUG_ON(ret < 0);

	return 0;
}

int avz_sched_yield(void)
{
	int ret;

	ret = hypercall_trampoline(__HYPERVISOR_sched_op, SCHEDOP_yield, 0, 0, 0);
	BUG_ON(ret < 0);

	return ret;
}


int avz_printk(char *buffer)
{
	int len;

	len = hypercall_trampoline(__HYPERVISOR_console_io, CONSOLEIO_write_string, 1, (long) buffer, 0);
	if (len < 0)
		BUG();

	return len;
}

