/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>

#include <asm/memory.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#include <soo/vbus.h>
#include <soo/gnttab.h>
#include <soo/paging.h>

#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>	/* hypercall defs */
#include <soo/vbstore.h>
#include <soo/uapi/console.h>

extern void unmap_kernel_range(unsigned long addr, unsigned long size);

#ifdef DEBUG
void gnttab_dump(void);
#endif

/*
 * Public grant-issuing interface functions
 */
int gnttab_grant_foreign_access(domid_t domid, unsigned long pfn)
{
        gnttab_op_t gnttab_op;

        /* Invoke the hypercall to grant access to domid */
        gnttab_op.cmd = GNTTAB_grant_page;
	gnttab_op.domid = domid;
        gnttab_op.pfn = pfn;
	
	avz_gnttab(&gnttab_op);

	return gnttab_op.ref;
}

/**
 * @brief Query the hypervisor to retrieve a pfn from a grant reference and
 *        do a mapping and return a vaddr
 * 
 * @param gnttab_op 
 */
void gnttab_map(domid_t domid, grant_ref_t grant_ref, void **vaddr) {
        struct vm_struct *area;
      	gnttab_op_t gnttab_op;

        gnttab_op.cmd = GNTTAB_map_page;
        gnttab_op.domid = domid;
        gnttab_op.ref = grant_ref;

        avz_gnttab(&gnttab_op);

        area = get_vm_area(PAGE_SIZE, VM_IOREMAP);
        BUG_ON(!area);
        
        paging_remap_page_range((unsigned long) area->addr, (unsigned long) area->addr + PAGE_SIZE, __pfn_to_phys(gnttab_op.pfn));      

        *vaddr = area->addr;
}

void gnttab_unmap(void *vaddr) {
        struct vm_struct *area;

        area = find_vm_area(vaddr);
        BUG_ON(!area);

        free_vm_area(area);	 

        /* Invalidate the mapping */
        unmap_kernel_range((unsigned long) vaddr, PAGE_SIZE);
}
