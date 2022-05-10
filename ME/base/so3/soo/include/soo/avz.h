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

#ifndef __AVZ_H__
#define __AVZ_H__


#include <soo/soo.h>

#include <asm/atomic.h>

/*
 * AVZ HYPERCALLS
 */

#define __HYPERVISOR_set_callbacks         0
#define __HYPERVISOR_event_channel_op      1
#define __HYPERVISOR_console_io            2
#define __HYPERVISOR_physdev_op            3
#define __HYPERVISOR_sched_op              4
#define __HYPERVISOR_domctl                5
#define __HYPERVISOR_soo_hypercall         6

/*
 * VIRTUAL INTERRUPTS
 *
 * Virtual interrupts that a guest OS may receive from the hypervisor.
 *
 */
#define VIRQ_TIMER      0  /* System timer tick virtualized interrupt */

/**************************************************/

/*
 * Commands to HYPERVISOR_console_io().
 */
#define CONSOLEIO_write_string  0
#define CONSOLEIO_process_char  1

typedef uint16_t domid_t;

/* Idle domain. */
#define DOMID_IDLE (0x7FFFU)

/* DOMID_SELF is used in certain contexts to refer to oneself. */
#define DOMID_SELF (0x7FF0U)

/* Agency */
#define DOMID_AGENCY	0

/* Realtime agency subdomain */
#define DOMID_AGENCY_RT	1

#define DOMID_INVALID (0x7FF4U)

/*
 * 128 event channels per domain
 */
#define NR_EVTCHN 128

/*
 * avz/domain shared data -- pointer provided in start_info.
 */
struct shared_info {

	uint8_t evtchn_upcall_pending;

	/*
	 * Updates to the following values are preceded and followed by an
	 * increment of 'version'. The guest can therefore detect updates by
	 * looking for changes to 'version'. If the least-significant bit of
	 * the version number is set then an update is in progress and the guest
	 * must wait to read a consistent set of values.
	 * The correct way to interact with the version number is similar to
	 * ME's seqlock: see the implementations of read_seqbegin/read_seqretry.
	 */
	uint32_t version;
	uint64_t tsc_timestamp;   /* Current (local) TSC from the free-running clocksource */
	uint64_t tsc_prev;

	/*
	 * A domain can create "event channels" on which it can send and receive
	 * asynchronous event notifications.
	 * Each event channel is assigned a bit in evtchn_pending and its modification has to be
	 * kept atomic.
	 */

	volatile bool evtchn_pending[NR_EVTCHN];

	atomic_t dc_event;

	/* Clocksource reference used during migration */
	uint64_t clocksource_ref;
	uint64_t clocksource_base;

	/* Agency or ME descriptor */
	dom_desc_t dom_desc;

	struct shared_info *subdomain_shared_info;

	/* Reference to the logbool hashtable (one per each domain) */
	void *logbool_ht;
};

typedef struct shared_info shared_info_t;

extern volatile shared_info_t *HYPERVISOR_shared_info;

extern void hypercall_trampoline(int hcall, long a0, long a2, long a3, long a4);

#define avz_shared_info ((shared_info_t *) HYPERVISOR_shared_info)

/*
 * start_info structure
 */

struct start_info {

    int	domID;

    unsigned long nr_pages;     /* Total pages allocated to this domain.  */

    shared_info_t *shared_info;  /* AVZ virtual address of the shared info page */

    unsigned long hypercall_addr; /* Hypercall vector addr for direct branching without syscall */
    unsigned long fdt_paddr;

    /* Low-level print function mainly for debugging purpose */
    void (*printch)(char c);

    unsigned long store_mfn;    /* MACHINE page number of shared page.    */

    unsigned long nr_pt_frames; /* Number of bootstrap p.t. frames.       */
    unsigned long dom_phys_offset;

    unsigned long pt_vaddr;  /* Virtual address of the page table used when the domain is bootstraping */

    unsigned long logbool_ht_set_addr;  /* Address of the logbool ht_set function which can be used in the domain. */

    /* We inform the domain about the hypervisor memory region so that the
     * domain can re-map correctly.
     */
    addr_t hypervisor_vaddr;

};
typedef struct start_info start_info_t;

extern start_info_t *avz_start_info;

/*
 * DOMCALLs
 */
typedef int (*domcall_t)(int cmd, void *arg);

#define DOMCALL_sync_vbstore               	1
#define DOMCALL_post_migration_sync_ctrl    	2
#define DOMCALL_sync_domain_interactions    	3
#define DOMCALL_presetup_adjust_variables   	4
#define DOMCALL_postsetup_adjust_variables  	5
#define DOMCALL_fix_other_page_tables		6
#define DOMCALL_sync_directcomm			7
#define DOMCALL_soo				8

struct DOMCALL_presetup_adjust_variables_args {
	start_info_t *start_info_virt; /* IN */
	unsigned int clocksource_vaddr; /* IN */
};

struct DOMCALL_postsetup_adjust_variables_args {
	long pfn_offset;
};

struct DOMCALL_fix_page_tables_args {
	long pfn_offset; /* Offset with which to fix the page table entries */
	unsigned int min_pfn, nr_pages;   /* min_pfn is the physical start address of the target RAM, nr_pages the number of pages */
};

struct DOMCALL_directcomm_args {
	unsigned int directcomm_evtchn;
};

struct DOMCALL_sync_vbstore_args {
	unsigned int vbstore_pfn; 		/* OUT */
	unsigned int vbstore_revtchn; /* Agency side */
};

struct DOMCALL_sync_domain_interactions_args {
	unsigned int vbstore_pfn; 	/* IN */
	unsigned int vbstore_levtchn;
	shared_info_t *shared_info_page; /* IN */
};

#define ME_domID() (avz_start_info->domID)
#define ME_prev_domID() (avz_start_info->prev_domID)

void postmig_adjust_timer(void);

#endif /* __AVZ_H__ */

