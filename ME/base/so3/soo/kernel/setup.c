/*
 * Copyright (C) 2014-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <common.h>
#include <memory.h>
#include <heap.h>
#include <initcall.h>

#include <asm/cacheflush.h>
#include <asm/mmu.h>
#include <asm/setup.h>

#include <soo/hypervisor.h>
#include <soo/avz.h>
#include <soo/evtchn.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/gnttab.h>

#include <soo/debug/logbool.h>

/* Avoid large area on stack (limited to 1024 bytes */

unsigned char vectors_tmp[PAGE_SIZE];

/* Force the variable to be stored in .data section so that the BSS can be freely cleared.
 * The value is set during the head.S execution before clear_bss().
 */
start_info_t *avz_start_info;
uint32_t avz_dom_phys_offset;

volatile uint32_t *HYPERVISOR_hypercall_addr;
volatile shared_info_t *HYPERVISOR_shared_info;

void *__guestvectors = NULL;

int do_presetup_adjust_variables(void *arg)
{
	struct DOMCALL_presetup_adjust_variables_args *args = arg;

	/* Normally, avz_start_info virt address is retrieved from r12 at guest bootstrap (head.S)
	 * We need to readjust this address after migration.
	 */
	avz_start_info = args->start_info_virt;

	avz_dom_phys_offset = avz_start_info->dom_phys_offset;

	mem_info.phys_base = avz_dom_phys_offset;

	HYPERVISOR_hypercall_addr = (uint32_t *) avz_start_info->hypercall_addr;

	__printch = avz_start_info->printch;

	/* Adjust timer information */
	postmig_adjust_timer();

	return 0;
}

int do_postsetup_adjust_variables(void *arg)
{
	struct DOMCALL_postsetup_adjust_variables_args *args = arg;

	/* Updating pfns where used. */
	readjust_io_map(args->pfn_offset);

	vectors_setup();

	return 0;
}

/*
 * Map the vbstore shared page with the agency.
 */
static void map_vbstore_page(unsigned long vbstore_pfn, bool clear)
{

	/* Reset the L1 PTE so that we are ready to allocate a page for vbstore. */
	//clear_l1pte(NULL, CONFIG_VBSTORE_VADDR);

	/* Re-map the new vbstore page */
	create_mapping(NULL, CONFIG_VBSTORE_VADDR, pfn_to_phys(vbstore_pfn), PAGE_SIZE, true);

}

int do_sync_domain_interactions(void *arg)
{
#if 0
	struct DOMCALL_sync_domain_interactions_args *args = arg;
	pcb_t *pcb;
	uint32_t *l1pte, *l1pte_current;

	HYPERVISOR_shared_info = args->shared_info_page;

	map_vbstore_page(args->vbstore_pfn, false);

	l1pte_current = l1pte_offset(__sys_root_pgtable, CONFIG_VBSTORE_VADDR);

	list_for_each_entry(pcb, &proc_list, list)
	{
		clear_l1pte(pcb->pgtable, CONFIG_VBSTORE_VADDR);
		l1pte = l1pte_offset(pcb->pgtable, CONFIG_VBSTORE_VADDR);

		*l1pte = *l1pte_current;

		flush_pte_entry((void *) l1pte);
	}


	postmig_vbstore_setup(args);
#endif
	return 0;
}

/**
 * This function is called at early bootstrap stage along head.S.
 */
void avz_setup(void) {

	mem_info.phys_base = avz_dom_phys_offset;
	mem_info.size = avz_start_info->nr_pages << PAGE_SHIFT;

	__printch = avz_start_info->printch;

	avz_dom_phys_offset = avz_start_info->dom_phys_offset;

	/* Immediately prepare for hypercall processing */
	HYPERVISOR_hypercall_addr = (uint32_t *) avz_start_info->hypercall_addr;

	lprintk("SOO Agency Virtualizer (avz) Start info :\n\n");

	lprintk("- Virtual address of printch() function: %lx\n", __printch);
	lprintk("- Hypercall addr: %lx\n", (addr_t) HYPERVISOR_hypercall_addr);
	lprintk("- Shared info page addr: %lx\n", (addr_t) avz_start_info->shared_info);
	lprintk("- Dom phys offset: %lx\n\n", (addr_t) avz_dom_phys_offset);

	mem_info.size = avz_start_info->nr_pages * PAGE_SIZE;
	mem_info.phys_base = avz_dom_phys_offset;

	__ht_set = (ht_set_t) avz_start_info->logbool_ht_set_addr;

	lprintk("SO3 ME Domain phys base: %x for a size of 0x%x bytes.\n", mem_info.phys_base, mem_info.size);

	/* At this point, we are ready to set up the virtual addresses
 	   to access the shared info page */
	HYPERVISOR_shared_info = (shared_info_t *) avz_start_info->shared_info;

	DBG("Set HYPERVISOR_set_callbacks at %lx\n", (unsigned long) linux0_hypervisor_callback);

	hypercall_trampoline(__HYPERVISOR_set_callbacks, (unsigned long) avz_vector_callback, (unsigned long) domcall, 0, 0);

	virq_init();
}

void pre_irq_init_setup(void) {

	/* Create a private vector page for the guest vectors */
	 __guestvectors = memalign(PAGE_SIZE, PAGE_SIZE);
	BUG_ON(!__guestvectors);

	vectors_setup();
}

void post_init_setup(void) {

	printk("VBstore shared page with agency at pfn 0x%x\n", avz_start_info->store_mfn);
	map_vbstore_page(avz_start_info->store_mfn, false);

	printk("SOO Mobile Entity booting ...\n");

	soo_guest_activity_init();

	callbacks_init();

	/* Initialize the Vbus subsystem */
	vbus_init();

	gnttab_init();

	/*
	 * Now, the ME requests to be paused by setting its state to ME_state_preparing. As a consequence,
	 * the agency will pause it.
	 */
	set_ME_state(ME_state_preparing);

	/*
	 * There are two scenarios.
	 * 1. Classical injection scheme: Wait for the agency to perform the pause+unpause. It should set the ME
	 *    state to ME_state_booting to allow the ME to continue.
	 * 2. ME that has migrated on a Smart Object: The ME state is ME_state_migrating, so it is different from
	 *    ME_state_preparing.
	 */

	while (1) {
		schedule();

		if (get_ME_state() != ME_state_preparing) {
			DBG("ME state changed: %d, continuing...\n", get_ME_state());
			break;
		}
	}

	/* Write the entries related to the ME ID in vbstore */
	vbstore_ME_ID_populate();

	/* How create all vbstore entries required by the frontend drivers */
	vbstore_init_dev_populate();

	printk("SO3  Operating System -- Copyright (c) 2016-2022 REDS Institute (HEIG-VD)\n\n");

	DBG("ME running as domain %d\n", ME_domID());
}

REGISTER_PRE_IRQ_INIT(pre_irq_init_setup)
REGISTER_POSTINIT(post_init_setup)
