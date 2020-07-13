/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/memslot.h>
#include <asm/io.h>
#include <asm/mmu.h>

#include <avz/lib.h>
#include <avz/smp.h>

#include <avz/types.h>
#include <avz/console.h>
#include <avz/migration.h>
#include <avz/domain.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/logbool.h>

#include <soo_migration.h>

extern void switch_domain_address_space(struct domain *from, struct domain *to);
long evtchn_bind_existing_interdomain(struct domain *ld, struct domain *remote, int lport, int rport);

extern long pfn_offset;

/**
 * Initiate the last stage of the migration process of a ME, so called "migration finalization".
 */
int migration_final(soo_hyp_t *op) {
	int rc;
	unsigned int slotID = *((unsigned int *) op->p_val1);
	soo_personality_t pers = *((soo_personality_t *) op->p_val2);
	struct domain *domME = domains[slotID];

	DBG("Personality: %d, ME state: %d\n", pers, get_ME_state(slotID));

	switch (pers) {
	case SOO_PERSONALITY_INITIATOR:
		DBG("Initiator\n");

		if (get_ME_state(slotID) != ME_state_dormant)
			domain_unpause_by_systemcontroller(domME);

		break;

	case SOO_PERSONALITY_TARGET:
		DBG("Target\n");

		flush_all();

		if ((rc = restore_migrated_domain(slotID)) < 0) {
			printk("Agency: %s:%d Failed to restore migrated domain (%d)\n", __func__, __LINE__, rc);
			BUG();
			return rc;
		}

		break;

	case SOO_PERSONALITY_SELFREFERENT:
		DBG("Self-referent\n");

		flush_all();

		DBG0("ME paused OK\n");

		if ((rc = restore_injected_domain(slotID)) < 0) {
			printk("Agency: %s:%d Failed to restore injected domain (%d)\n", __func__, __LINE__, rc);
			BUG();
			return rc;
		}

		break;

	default:
		printk("Agency: %s:%d Invalid personality value (%d)\n", __func__, __LINE__, pers);
		BUG();

		break;
	}

	return 0;
}


/*------------------------------------------------------------------------------
 fix_page_table_ME
 Fix ME kernel page table (swapper_pg_dir) to fit new physical address space.
 We only fix the first MBs so that kernel static mapping is fixed + the
 hypervisor mapped addresses (@ 0xFF000000) so that DOMCALLs can work.
 The rest of the page table will get fixed directly in the ME using a DOMCALL.
 ------------------------------------------------------------------------------*/
extern unsigned long vaddr_start_ME;
static void fix_kernel_boot_page_table_ME(unsigned int ME_slotID, ME_type_t ME_type)
{
	struct domain *me = domains[ME_slotID];
	pde_t *pgtable_ME;
	pde_t *pgd, *pgd_current;
	pmd_t *pmd;
	unsigned long vaddr;
	unsigned long old_pfn;
	unsigned long new_pfn;
	uint32_t prot_sect;
	unsigned long offset;
	volatile unsigned int base;

	/* SO3-related */
	uint32_t *l1pte, *l2pte, *l1pte_current;
	int i, j;

	/* The page table is found at domain_start + 0x4000 */
	pgtable_ME = (pde_t *) (vaddr_start_ME + 0x4000);

	/* According to the ME type, the initial fix for L1 page table may differ according
	 * to the internal organization of the page table.
	 */
	if (ME_type == ME_type_SO3) {

		/* Get the L1 PTE. */

		/* We re-adjust the PTE entries until the IO_MAPPING_BASE; the rest will be done by the domain */
		for (i = 0xc00; i < 0xe00; i++) {
			l1pte = (uint32_t *) pgtable_ME + i;
			if (!*l1pte)
				continue ;

			if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT) {

				old_pfn = (*l1pte & L1_SECT_MASK) >> PAGE_SHIFT;

				new_pfn = old_pfn + pfn_offset;

				/* If we have a section PTE, it means that pfn_offset *must* be 1 MB aligned */
				BUG_ON(((new_pfn << PAGE_SHIFT) & ~L1_SECT_MASK) != 0);

				*l1pte = (*l1pte & ~L1_SECT_MASK) | (new_pfn << PAGE_SHIFT);

				flush_pmd_entry((pde_t *) l1pte);

			} else {
				/* Fix the pfn of the 1st-level PT */
				base = (*l1pte & L1DESC_L2PT_BASE_ADDR_MASK);
				base += pfn_to_phys(pfn_offset);
				*l1pte = (*l1pte & ~L1DESC_L2PT_BASE_ADDR_MASK) | base;

				flush_pmd_entry((pde_t *) l1pte);

				for (j = 0; j < 256; j++) {

					l2pte = ((uint32_t *) __lva(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK)) + j;
					if (*l2pte) {

						/* Re-adjust the pfn of the L2 PTE */
						base = *l2pte & PAGE_MASK;
						base += pfn_to_phys(pfn_offset);
						*l2pte = (*l2pte & ~PAGE_MASK) | base;

						flush_pmd_entry((pde_t *) l2pte);
					}

				}
			}
		}

		/* Now, adjust the I/O range generally used by local UART - Used by AVZ */
		for (i = 0xf80; i < 0xf8f; i++) {
			l1pte = (uint32_t *) pgtable_ME + i;
			l1pte_current = (uint32_t *) swapper_pg_dir + i;

			*l1pte = *l1pte_current;
			flush_pmd_entry((pde_t *) l1pte);
		}

	} else {

		/* ME of Linux type */

		/* Fix the first 6 MB of kernel code */
		for (vaddr = 0xC0000000; vaddr < 0xC0600000; vaddr += SECTION_SIZE) {
			pgd = pgd_offset_priv(pgtable_ME, vaddr);

			pmd = (pmd_t *) pmd_offset(pgd);

			if (pmd_none(*pmd))
				continue;

			if ((pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT) {

				/* Get section info to restore them after the fix */
				prot_sect = pmd_val(*pmd) & ~PMD_MASK;

				/* Get the address part */
				old_pfn = __phys_to_pfn(pmd_val(*pmd) & PMD_MASK);

				new_pfn = old_pfn + pfn_offset;
				*pmd = __pmd(__pfn_to_phys(new_pfn) | prot_sect);

				flush_pmd_entry((pde_t *) pmd);
			} else {
				printk("%s: unhandled pmd : %lx PMD_TYPE (%ld) for vaddr 0x%08lX!\n", __FUNCTION__, pmd_val(*pmd), (pmd_val(*pmd) & PMD_TYPE_MASK), vaddr);
				BUG();
			}
		}
	}

	/* Fix the Hypervisor mapped addresses (size of hyp = 12 MB) */
	for (vaddr = 0xFF000000; vaddr < 0xFFC00000; vaddr += SECTION_SIZE) {
		pgd = pgd_offset_priv(pgtable_ME, vaddr);
		pgd_current = pgd_offset_priv(swapper_pg_dir, vaddr);

		pgd->l2 = pgd_current->l2;
		flush_pmd_entry((pde_t *) pgd);
	}

	/* Fix the physical address of the ME kernel page table */
	me->vcpu[0]->arch.guest_table.pfn += pfn_offset;

	/* Fix other phys. var. such as TTBR* */

	/* Preserve the low-level bits like SMP related bits */
	offset = me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr0 & ((1 << PAGE_SHIFT) - 1);

	old_pfn = __phys_to_pfn(me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr0);

	me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr0 = __pfn_to_phys(old_pfn + pfn_offset) + offset;

	offset = me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr1 & ((1 << PAGE_SHIFT) - 1);

	if (me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr1 != 0) {
		old_pfn = __phys_to_pfn(me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr1);
		me->vcpu[0]->arch.guest_context.sys_regs.guest_ttbr1 = __pfn_to_phys(old_pfn + pfn_offset) + offset;
	}

	/* Flush all cache */
	flush_all();
}

/*------------------------------------------------------------------------------
 fix_kernel_page_tables_ME
 Fix the primary system page table (swapper_pg_dir) in ME
 We pass the current domain as argument as we need it to make the DOMCALLs.
 ------------------------------------------------------------------------------*/
static int fix_kernel_page_table_ME(unsigned int ME_slotID, struct domain *current_dom)
{
	int rc = 0;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_fix_page_tables_args fix_pt_args;
	unsigned char vectors[PAGE_SIZE];

	fix_pt_args.pfn_offset = pfn_offset;

	fix_pt_args.min_pfn =  ((start_info_t *) me->arch.vstartinfo_start)->min_mfn;
	fix_pt_args.nr_pages = ((start_info_t *) me->arch.vstartinfo_start)->nr_pages;

	DBG("DOMCALL_fix_page_tables called in ME with pfn_offset=%ld (%lx)\n", fix_pt_args.pfn_offset, fix_pt_args.pfn_offset);

	rc = domain_call(me, DOMCALL_fix_kernel_page_table, &fix_pt_args, current_dom);
	if (rc != 0) {
		printk("DOMCALL_fix_page_tables FAILED!\n");
		goto out;
	}

	/* We still need to re-adjust the ARM vectors which have to go the hypervisor ISRs */
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

	/* Save a backup of the vectors in order to make a copy later on, in the right guest pages */
	memcpy(vectors, (void *) 0xffff0000, PAGE_SIZE);

	switch_mm(idle_domain[smp_processor_id()]->vcpu[0], me->vcpu[0]);

	memcpy((void *) 0xffff0000, vectors, 0x20); /* We restore the vectors; they must be those of the hypervisor */
	memcpy((void *) 0xffff0000 + 0x200, vectors + 0x200, 0x300); /* Vector stubs */

	dmb();

	/* Flush all cache */
	flush_all();

	switch_mm(me->vcpu[0], NULL);

	out:
	return rc;
}

/*------------------------------------------------------------------------------
 fix_page_tables_ME
 Fix all page tables in ME (swapper_pg_dir + all processes)
 We pass the current domain as argument as we need it to make the DOMCALLs.
 ------------------------------------------------------------------------------*/
static int fix_other_page_tables_ME(unsigned int ME_slotID, struct domain *current_dom)
{
	int rc = 0;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_fix_page_tables_args fix_pt_args;

	fix_pt_args.pfn_offset = pfn_offset;

	fix_pt_args.min_pfn =  ((start_info_t *) me->arch.vstartinfo_start)->min_mfn;
	fix_pt_args.nr_pages = ((start_info_t *) me->arch.vstartinfo_start)->nr_pages;

	DBG("DOMCALL_fix_other_page_tables called in ME with pfn_offset=%ld (%lx)\n", fix_pt_args.pfn_offset, fix_pt_args.pfn_offset);

	rc = domain_call(me, DOMCALL_fix_other_page_tables, &fix_pt_args, current_dom);
	if (rc != 0) {
		printk("DOMCALL_fix_page_tables FAILED!\n");
		goto out;
	}

	/* Flush all cache */
	flush_all();

out:
	return rc;
}

/*------------------------------------------------------------------------------
 sync_directcomm
 This function updates the directcomm event channel in both domains
 ------------------------------------------------------------------------------*/
static int rebind_directcomm(unsigned int ME_slotID, struct domain *cur_dom)
{
	int rc;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_directcomm_args agency_directcomm_args, ME_directcomm_args;

	DBG("%s\n", __FUNCTION__);


	/* Get the directcomm evtchn from agency */

	memset(&agency_directcomm_args, 0, sizeof(struct DOMCALL_directcomm_args));

	/* Pass the (remote) domID in directcomm_evtchn */
	agency_directcomm_args.directcomm_evtchn = ME_slotID;

	rc = domain_call(agency, DOMCALL_sync_directcomm, &agency_directcomm_args, cur_dom);
	if (rc != 0) {
		printk("DOMCALL_get_directcomm_info to agency FAILED!\n");
		goto out;
	}

	memset(&ME_directcomm_args, 0, sizeof(struct DOMCALL_directcomm_args));

	/* Pass the domID in directcomm_evtchn */
	ME_directcomm_args.directcomm_evtchn = 0;

	rc = domain_call(me, DOMCALL_sync_directcomm, &ME_directcomm_args, cur_dom);
	if (rc != 0) {
		printk("DOMCALL_get_directcomm_info to ME FAILED!\n");
		goto out;
	}

	DBG("%s: Rebinding directcomm event channels: %d (agency) <-> %d (ME)\n", __func__, agency_directcomm_args.directcomm_evtchn, ME_directcomm_args.directcomm_evtchn);

	rc = evtchn_bind_existing_interdomain(me, agency, ME_directcomm_args.directcomm_evtchn, agency_directcomm_args.directcomm_evtchn);

	if (rc != 0) {
		printk("evtchn_bind_existing_interdomain(ME, %d -> %d) FAILED!\n", ME_directcomm_args.directcomm_evtchn, agency_directcomm_args.directcomm_evtchn);
		goto out;
	}

	/* Success */
	rc = 0;

	out: return rc;
}

/*------------------------------------------------------------------------------
 sync_domain_interactions
 - Create the mmory mappings in ME which are normally done at boot time
   This is done using DOMCALLs. We first have to retrieve info from agency
   using DOMCALLs as well.
   We pass the current domain as argument as we need it to make the DOMCALLs.
 - Performs the rebinding of vbstore event channel
 ------------------------------------------------------------------------------*/
static int sync_domain_interactions(unsigned int ME_slotID, struct domain *current_dom)
{
	int rc;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_sync_vbstore_args xs_args;
	struct DOMCALL_sync_domain_interactions_args sync_args;

	memset(&xs_args, 0, sizeof(struct DOMCALL_sync_vbstore_args));

	/* Retrieve ME vbstore info from the agency */

	/* Pass the ME_domID in vbstore_remote_ME_evtchn field */
	xs_args.vbstore_revtchn = me->domain_id;

	rc = domain_call(agency, DOMCALL_sync_vbstore, &xs_args, current_dom);
	if (rc != 0) {
		printk("DOMCALL_get_vbstore_info FAILED!\n");
		goto out;
	}

	/* Create the mappings in ME */
	sync_args.vbstore_pfn = xs_args.vbstore_pfn;
	sync_args.shared_info_page = me->shared_info;

	rc = domain_call(me, DOMCALL_sync_domain_interactions, &sync_args, current_dom);
	if (rc != 0) {
		printk("DOMCALL_create_mem_mappings FAILED!\n");
		goto out;
	}

	/*
	 * Rebinding the event channel used to access vbstore in agency
	 */
	DBG("%s: Rebinding vbstore event channels: %d (agency) <-> %d (ME)\n", __func__, xs_args.vbstore_revtchn, sync_args.vbstore_levtchn);

	rc = evtchn_bind_existing_interdomain(me, agency, sync_args.vbstore_levtchn, xs_args.vbstore_revtchn);

	if (rc != 0) {
		printk("%s: rebinding vbstore event channel %d (agency) <-> %d (ME) FAILED!\n", __func__, xs_args.vbstore_revtchn, sync_args.vbstore_levtchn);
		goto out;
	}

	rebind_directcomm(ME_slotID, current_dom);

	out:
	return rc;
}

/*------------------------------------------------------------------------------
 adjust_variables_in_ME
 Adjust variables such as start_info in ME
 ------------------------------------------------------------------------------*/
static int presetup_adjust_variables_in_ME(unsigned int ME_slotID, start_info_t *start_info_virt, struct domain *cur_dom)
{
	int rc;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_presetup_adjust_variables_args adjust_variables;

	adjust_variables.start_info_virt = start_info_virt;
	adjust_variables.clocksource_vaddr = (unsigned int) system_timer_clocksource->vaddr;

	rc = domain_call(me, DOMCALL_presetup_adjust_variables, &adjust_variables, cur_dom);
	if (rc != 0)
		goto out;

	/* Success */
	rc = 0;

	out: return rc;
}

/*------------------------------------------------------------------------------
 adjust_variables_in_ME
 Adjust variables such as start_info in ME
 ------------------------------------------------------------------------------*/
static int postsetup_adjust_variables_in_ME(unsigned int ME_slotID, struct domain *cur_dom)
{
	int rc;
	struct domain *me = domains[ME_slotID];
	struct DOMCALL_postsetup_adjust_variables_args adjust_variables;

	adjust_variables.pfn_offset = pfn_offset;

	rc = domain_call(me, DOMCALL_postsetup_adjust_variables, &adjust_variables, cur_dom);
	if (rc != 0)
		goto out;

	/* Success */
	rc = 0;

	out: return rc;
}

int restore_migrated_domain(unsigned int ME_slotID) {
	int rc;
	struct domain *me = NULL;

	DBG("Restoring migrated domain on ME_slotID: %d\n", ME_slotID);

	/* Switch to idle domain address space which has a full mapping of the RAM */
	switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

	me = domains[ME_slotID];

	/* Restore domain info received from Client */
	mig_restore_domain_migration_info(ME_slotID, me);

	/* Restore VCPU info received from Client */
	mig_restore_vcpu_migration_info(ME_slotID, me);

	/* Init post-migration execution of ME */

	/* Stack pointer (r13) should remain unchanged since on the receiver side we did not make any push on the SVC stack */
	me->vcpu[0]->arch.guest_context.user_regs.r13 = (unsigned long) setup_dom_stack(me->vcpu[0]);

	/* Setting the (future) value of PC in r14 (LR). See code switch_to in entry-armv.S */
	me->vcpu[0]->arch.guest_context.user_regs.r14 = (unsigned int) (void *) after_migrate_to_user;

	/* Issue a timer interrupt (first timer IRQ) avoiding some problems during the forced upcall in after_migrate_to_user */
	send_timer_event(me->vcpu[0]);

	/* Fix the ME kernel page table for domcalls to work */
	fix_kernel_boot_page_table_ME(ME_slotID, me->shared_info->dom_desc.u.ME.type);

	/* Switch back to agency address space */
	switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

	DBG0("DOMCALL_presetup_adjust_variables_in_ME\n");
	/* Adjust variables in ME such as start_info */
	rc = presetup_adjust_variables_in_ME(ME_slotID, (struct start_info *) me->arch.vstartinfo_start, agency);
	if (rc != 0)
		goto out_error;

	/* Fix the kernel page table in the ME */
	DBG("%s: fix kernel page table in the ME...\n", __func__);
	rc = fix_kernel_page_table_ME(ME_slotID, agency);
	if (rc != 0)
		goto out_error;

	/* Fix all page tables in the ME (all processes) via a domcall */
	DBG("%s: fix other page tables in the ME...\n", __func__);
	rc = fix_other_page_tables_ME(ME_slotID, agency);
	if (rc != 0)
		goto out_error;

	DBG0("DOMCALL_postsetup_adjust_variables_in_ME\n");

	/* Adjust variables in the ME such as re-adjusting pfns */
	rc = postsetup_adjust_variables_in_ME(ME_slotID, agency);
	if (rc != 0)
		goto out_error;

	/*
	 * Perform synchronization work like memory mappings & vbstore event channel restoration.
	 *
	 * Create the memory mappings in the ME that are normally done at boot
	 * time. We pass the current domain needed by the domcalls to correctly
	 * switch between address spaces */


	DBG("%s: syncing domain interactions in agency...\n", __func__);
	rc = sync_domain_interactions(ME_slotID, agency);
	if (rc != 0)
		goto out_error;

	/* We've done as much initialisation as we could here. */

	ASSERT(smp_processor_id() == 0);

	/* Proceed with the SOO post-migration callbacks according to patent */

	/* Pre-activate */
	rc = soo_pre_activate(ME_slotID);
	if (rc != 0)
		goto out_error;

	/*
	 * We check if the ME has been killed during the pre_activate callback.
	 * If yes, we do not pursue our re-activation process.
	 */
	if (get_ME_state(ME_slotID) == ME_state_dead)
		return 0;

	ASSERT(smp_processor_id() == 0);

	/*
	 * Cooperate.
	 * We look for residing MEs which are ready to collaborate.
	 */

	rc = soo_cooperate(ME_slotID);
	if (rc != 0)
		goto out_error;

	/*
	 * We check if the ME has been killed or set to the dormant state during the cooperate
	 * callback. If yes, we do not pursue our re-activation process.
	 */
	if ((get_ME_state(ME_slotID) == ME_state_dead) || (get_ME_state(ME_slotID) == ME_state_dormant))
		return 0;

	/* Are the ME still alive ? */
	if (domains[ME_slotID] == NULL)
		return 0;

	/* Resume ... */

	/* All sync-ed! Kick the ME alive! */

	ASSERT(smp_processor_id() == 0);

	DBG("%s: Now, resuming ME...\n", __func__);

	/* Give a new clocksource base (current) to this ME so that it can pursue its time-based activities */
	me->shared_info->clocksource_base  = system_timer_clocksource->read();

	domain_unpause_by_systemcontroller(me);

	/* Success */
	return 0;

out_error:

	/* Cleanup */
	if (me != NULL)
		free_domain_struct(me);

	DBG("%s failed!\n", __FUNCTION__);
	return -1;
}

int restore_injected_domain(unsigned int ME_slotID) {
	int rc;
	struct domain *me = domains[ME_slotID];

	DBG("Right before cooperate()...\n");

	if ((rc = soo_cooperate(me->domain_id)) < 0) {
		printk("Agency: %s:%d Failed to run cooperate (%d)\n", __func__, __LINE__, rc);
		BUG();
	}

	DBG("Right after cooperate()...\n");

	/* Are the ME still alive ? */
	if (domains[ME_slotID] == NULL)
		return 0;

	return 0;
}

