/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef __MIGRATION_H__
#define __MIGRATION_H__

#include <sched.h>

#include <soo/uapi/avz.h>

struct vcpu_migration_info
{
	int vcpu_id;

	int processor;

	bool need_periodic_timer;

	unsigned long pause_flags;
	atomic_t      pause_count;

	/* IRQ-safe virq_lock protects against delivering VIRQ to stale evtchn. */
	u16           virq_to_evtchn[NR_VIRQS];

	/* arch_vcpu structure */
	struct arch_vcpu arch;

	/* Internal fields of vcpu_info_t structure */
	uint8_t       evtchn_upcall_pending;
	uint8_t       evtchn_upcall_mask;
	unsigned long evtchn_pending_sel;

	struct arch_vcpu_info arch_info;
	struct vcpu_time_info time;
};

struct domain_migration_info
{
	domid_t domain_id;

	/*
	 *  Event channel struct information.
	 */
	struct evtchn evtchn[NR_EVTCHN];

	/*
	 * Interrupt to event-channel mappings. Updates should be protected by the
	 * domain's event-channel spinlock. Read accesses can also synchronise on
	 * the lock, but races don't usually matter.
	 */
	unsigned int nr_pirqs;

	unsigned long evtchn_pending[sizeof(unsigned long) * 8];
	unsigned long evtchn_mask[sizeof(unsigned long) * 8];
	u64 clocksource_ref;

	/* Start info page */
	unsigned char start_info_page[PAGE_SIZE];

	dom_desc_t dom_desc;

	/* Domain start pfn */
	unsigned long start_pfn;

	atomic_t pause_count;
};

#endif /* __MIGRATION_H__ */
