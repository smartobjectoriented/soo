/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <memory.h>
#include <heap.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>
#include <asm/armv7.h>

#include <mach/domcall.h>

#include <soo/hypervisor.h>
#include <soo/avz.h>
#include <soo/evtchn.h>
#include <soo/soo.h>
#include <soo/console.h>

#include <soo/debug/logbool.h>

/* Avoid large area on stack (limited to 1024 bytes */

unsigned char vectors_tmp[PAGE_SIZE];

/* Force the variable to be stored in .data section so that the BSS can be freely cleared.
 * The value is set during the head.S execution before clear_bss().
 */
start_info_t *avz_start_info = (start_info_t *) 0xbeef;

volatile shared_info_t *HYPERVISOR_shared_info;
volatile uint32_t *HYPERVISOR_hypercall_addr;
uint32_t avz_guest_phys_offset = 0xbeef;

void *__guestvectors = NULL;

extern uint8_t _vectors[];


extern void inject_syscall_vector(void);

int do_presetup_adjust_variables(void *arg)
{
	struct DOMCALL_presetup_adjust_variables_args *args = arg;

	/* We begin to configure this ME as a target-personality */
	soo_set_personality(SOO_PERSONALITY_TARGET);

	/* Normally, avz_start_info virt address is retrieved from r12 at guest bootstrap (head.S)
	 * We need to readjust this address after migration.
	 */
	avz_start_info = args->start_info_virt;

	avz_guest_phys_offset = avz_start_info->min_mfn << PAGE_SHIFT;

	mem_info.phys_base = avz_guest_phys_offset;

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

	/* We need to add handling of swi/svc software interrupt instruction for syscall processing.
	 * Such an exception is fully processed by the SO3 domain.
	 */
	inject_syscall_vector();

	flush_icache_range(VECTOR_VADDR, VECTOR_VADDR + PAGE_SIZE);

	return 0;
}

void vectors_setup(void) {
	void *vectors;

 	/* Create a private vector page for the guest vectors */
 	__guestvectors = memalign(PAGE_SIZE, PAGE_SIZE);
 	BUG_ON(!__guestvectors);

 	/* Create a local page for original vectors. */
 	vectors = memalign(PAGE_SIZE, PAGE_SIZE);
 	BUG_ON(!vectors);

 	/* Make a copy of the existing vectors. The L2 pagetable was allocated by AVZ and cannot be used as such by the guest.
 	 * Therefore, we will make our own mapping in the guest for this vector page.
 	 */
 	memcpy(vectors_tmp, (void *) VECTOR_VADDR, PAGE_SIZE);

 	/* Reset the L1 PTE used for the vector page. */
 	clear_l1pte(NULL, VECTOR_VADDR);

 	create_mapping(NULL, VECTOR_VADDR, __pa((uint32_t) vectors), PAGE_SIZE, true, false);

 	memcpy((void *) VECTOR_VADDR, vectors_tmp, PAGE_SIZE);

	/* Create the guest vector pages at 0xffff5000 in order to have a good mapping with all required privileges */
	create_mapping(NULL, GUEST_VECTOR_VADDR, __pa((uint32_t) __guestvectors), PAGE_SIZE, true, false);

	/* From now on... */
	__guestvectors = (unsigned int *) GUEST_VECTOR_VADDR;

	memcpy(__guestvectors, (void *) _vectors, PAGE_SIZE);

	/* We need to add handling of swi/svc software interrupt instruction for syscall processing.
	 * Such an exception is fully processed by the SO3 domain.
	 */
	inject_syscall_vector();

	flush_icache_range(VECTOR_VADDR, VECTOR_VADDR + PAGE_SIZE);
}

/*
 * Map the vbstore shared page with the agency.
 */
static void map_vbstore_page(unsigned long vbstore_pfn, bool clear)
{
	if (clear)
		/* Release the previous mapping */
		release_mapping(NULL, HYPERVISOR_VBSTORE_VADDR, PAGE_SIZE);
	else
		/* Reset the L1 PTE so that we are ready to allocate a page for vbstore. */
		clear_l1pte(NULL, HYPERVISOR_VBSTORE_VADDR);

	/* Re-map the new vbstore page */
	create_mapping(NULL, HYPERVISOR_VBSTORE_VADDR, pfn_to_phys(vbstore_pfn), PAGE_SIZE, true, false);
}

int do_sync_domain_interactions(void *arg)
{
	struct DOMCALL_sync_domain_interactions_args *args = arg;
	HYPERVISOR_shared_info = args->shared_info_page;

	map_vbstore_page(args->vbstore_pfn, true);
	postmig_vbstore_setup(args);

	return 0;
}

void board_setup_post(void) {
	vectors_setup();

	printk("VBstore shared page with agency at pfn 0x%x\n", avz_start_info->store_mfn);
	map_vbstore_page(avz_start_info->store_mfn, false);
}

void board_setup(void) {
	int ret;

	__printch = avz_start_info->printch;

	/* Immediately prepare for hypercall processing */
	HYPERVISOR_hypercall_addr = (uint32_t *) avz_start_info->hypercall_addr;

	printk("SOO Agency Virtualizer (avz) Start info :\n");
	printk("Hypercall addr: %x\n", (uint32_t) HYPERVISOR_hypercall_addr);
	printk("Shared info page addr: %x\n", (uint32_t) avz_start_info->shared_info);

	mem_info.size = avz_start_info->nr_pages * PAGE_SIZE;
	mem_info.phys_base = avz_guest_phys_offset;

	__ht_set = (ht_set_t) avz_start_info->logbool_ht_set_addr;

	printk("SO3 ME Domain phys base: %x for a size of 0x%x bytes.\n", mem_info.phys_base, mem_info.size);

	/* At this point, we are ready to set up the virtual addresses
 	   to access the shared info page */
	HYPERVISOR_shared_info = (shared_info_t *) avz_start_info->shared_info;

	DBG("Set HYPERVISOR_set_callbacks at %lx\n", (unsigned long) linux0_hypervisor_callback);

	ret = hypercall_trampoline(__HYPERVISOR_set_callbacks, (unsigned long) avz_vector_callback, (unsigned long) domcall, 0, 0);
	BUG_ON(ret < 0);

	virq_init();
}


