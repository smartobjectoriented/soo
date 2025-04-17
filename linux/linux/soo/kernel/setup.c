/*
 * Copyright (C) 2014-2021 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/mm_types.h>

#include <asm/page.h>

#ifdef CONFIG_ARM
#include <asm/cp15.h>
#endif

#include <asm/cacheflush.h>

#include <soo/hypervisor.h>
#include <soo/avz.h>

#include <soo/uapi/console.h>

volatile unsigned long *HYPERVISOR_hypercall_addr;

/* Updated in kernel/head.S */
volatile avz_shared_t *__avz_shared;

void __init avz_setup(void)
{

	lprintk("  - SOO Agency Virtualizer (avz) Start info :\n");
	lprintk("  - Hypercall addr: %lx\n", (unsigned long) HYPERVISOR_hypercall_addr);
	lprintk("  - Total Pages allocated to this domain : %ld\n", avz_shared->nr_pages);
	lprintk("  - FDT device tree paddr, if any: %lx\n", avz_shared->fdt_paddr);
	
	lprintk("  - All right! AVZ setup successfull.\n");
}
