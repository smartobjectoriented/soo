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
#include <linux/memblock.h>

#include <asm/pgtable.h>

#include <soo/hypervisor.h>
#include <soo/evtchn.h>
#include <soo/paging.h>

#include <soo/uapi/avz.h>

/*
 * SOO hypercall
 *
 * Mandatory arguments:
 * - cmd: hypercall
 * - addr: a virtual address used within the hypervisor
 * - p_val1: a (virtual) address to a first value
 * - p_val2: a (virtual) address to a second value
 */

void avz_hypercall(avz_hyp_t *avz_hyp)
{
        avz_hyp_t *__avz_hyp;

	/* Make sure the avz_hyp details are in a linear-mapped zone
	 * to be able to pass the physical address to the hypervisor.
	 */
	__avz_hyp = kmalloc(sizeof(avz_hyp_t), GFP_KERNEL);
        BUG_ON(!__avz_hyp);

        memcpy(__avz_hyp, avz_hyp, sizeof(avz_hyp_t));

        __flush_dcache_area((void *) __avz_hyp, sizeof(avz_hyp_t));
        __avz_hypercall(AVZ_HYPERCALL_TRAP, virt_to_phys(__avz_hyp));
        __inval_dcache_area((void *) __avz_hyp, sizeof(avz_hyp_t));

        memcpy(avz_hyp, __avz_hyp, sizeof(avz_hyp_t));

        kfree(__avz_hyp);
}

void avz_ME_unpause(domid_t domain_id, grant_ref_t vbstore_grant_ref)
{
        avz_hyp_t args;

	lprintk("Trying to unpause ME domain %d...", domain_id);

        args.cmd = AVZ_DOMAIN_CONTROL_OP;

        args.u.avz_domctl_args.domctl.cmd = DOMCTL_unpauseME;
	args.u.avz_domctl_args.domctl.domain = domain_id;
        args.u.avz_domctl_args.domctl.u.vbstore_grant_ref = vbstore_grant_ref;

        avz_hypercall(&args);
}

#if defined(CONFIG_SOO)

void avz_get_shared(void) {
        avz_hyp_t args;

        args.cmd = AVZ_DOMAIN_CONTROL_OP;
        args.u.avz_domctl_args.domctl.cmd = DOMCTL_get_AVZ_shared;

        avz_hypercall(&args);

        BUG_ON(!args.u.avz_domctl_args.domctl.u.avz_shared_paddr);

        avz_shared = (volatile avz_shared_t *) paging_remap(args.u.avz_domctl_args.domctl.u.avz_shared_paddr, PAGE_SIZE);
	BUG_ON(!avz_shared);

	BUG_ON(!avz_shared->subdomain_shared_paddr);

	avz_shared->subdomain_shared = (avz_shared_t *) paging_remap(avz_shared->subdomain_shared_paddr, PAGE_SIZE);
	BUG_ON(!avz_shared->subdomain_shared);
}

void avz_printch(char c) {
        avz_hyp_t args;

        args.cmd = AVZ_CONSOLE_IO_OP;

        args.u.avz_console_io_args.console.cmd = CONSOLE_IO_PRINTCH;
        args.u.avz_console_io_args.console.u.c = c;

        avz_hypercall(&args);
}

void avz_printstr(char *s) {
        avz_hyp_t args;

        args.cmd = AVZ_CONSOLE_IO_OP;
        args.u.avz_console_io_args.console.cmd = CONSOLE_IO_PRINTSTR;

        strncpy(args.u.avz_console_io_args.console.u.str, s, CONSOLE_STR_MAX_LEN);
       
        avz_hypercall(&args);
}

void avz_gnttab(gnttab_op_t *op) {
        avz_hyp_t args;

        args.cmd = AVZ_GRANT_TABLE_OP;

        memcpy(&args.u.avz_gnttab_args.gnttab_op, op, sizeof(gnttab_op_t));
        avz_hypercall(&args);
        memcpy(op, &args.u.avz_gnttab_args.gnttab_op, sizeof(gnttab_op_t));
}

#endif

void avz_ME_pause(domid_t domain_id)
{
        avz_hyp_t args;

        lprintk("Trying to pause domain %d...", domain_id);

        args.cmd = AVZ_DOMAIN_CONTROL_OP;
        args.u.avz_domctl_args.domctl.cmd = DOMCTL_pauseME;
        args.u.avz_domctl_args.domctl.domain = domain_id;
        
	avz_hypercall(&args);
}

