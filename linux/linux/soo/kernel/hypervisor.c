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
#include <soo/paging.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/schedop.h>
#include <soo/uapi/domctl.h>
#include <soo/uapi/physdev.h>

void avz_ME_unpause(domid_t domain_id, addr_t vbstore_pfn)
{
        struct domctl *op;

        op = kzalloc(sizeof(struct domctl), GFP_KERNEL);
        BUG_ON(!op);

        lprintk("Trying to unpause ME domain %d...", domain_id);

        op->cmd = DOMCTL_unpauseME;

	op->domain = domain_id;

	op->u.unpause_ME.vbstore_pfn = vbstore_pfn;

	avz_hypercall(__HYPERVISOR_domctl, virt_to_phys(op), 0 ,0 ,0);
}

#if defined(CONFIG_SOO) && !defined(CONFIG_LINUXVIRT)

void avz_get_shared(void) {
	struct domctl *op;

	op = kmalloc(sizeof(struct domctl), GFP_KERNEL);
	BUG_ON(!op);

	op->cmd = DOMCTL_get_AVZ_shared;

	avz_hypercall(__HYPERVISOR_domctl, virt_to_phys(op), 0, 0, 0);

	BUG_ON(!op->u.avz_shared_paddr);

	avz_shared = (volatile avz_shared_t *) paging_remap(op->u.avz_shared_paddr, PAGE_SIZE);
	BUG_ON(!avz_shared);

	BUG_ON(!avz_shared->subdomain_shared_paddr);

	avz_shared->subdomain_shared = (avz_shared_t *) paging_remap(avz_shared->subdomain_shared_paddr, PAGE_SIZE);
	BUG_ON(!avz_shared->subdomain_shared);

}

void avz_printch(char c) {
	avz_hypercall(__HYPERVISOR_console_io, c, 0, 0, 0);
}

#endif

void avz_ME_pause(domid_t domain_id)
{
	struct domctl op;

	lprintk("Trying to pause domain %d...", domain_id);

	op.cmd = DOMCTL_pauseME;
	op.domain = domain_id;

	avz_hypercall(__HYPERVISOR_domctl, (long) &op, 0, 0, 0);
}

void avz_dump_page(unsigned int pfn)
{
	avz_hypercall(__HYPERVISOR_physdev_op, PHYSDEVOP_dump_page, (long) &pfn, 0, 0);
}

void avz_dump_logbool(void)
{
	avz_hypercall(__HYPERVISOR_physdev_op, PHYSDEVOP_dump_logbool, 0 ,0, 0);
}

void avz_send_IPI(int ipinr, long cpu_mask) {
	send_ipi_args_t send_ipi_args;

	send_ipi_args.ipinr = ipinr;
	send_ipi_args.cpu_mask = cpu_mask;

	avz_hypercall(__HYPERVISOR_physdev_op, PHYSDEVOP_send_ipi, (long) &send_ipi_args, 0, 0);
}

