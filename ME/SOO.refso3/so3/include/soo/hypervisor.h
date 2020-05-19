/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef _HYPERVISOR_H_
#define _HYPERVISOR_H_

#include <soo/avz.h>
#include <soo/physdev.h>

#define GUEST_VECTOR_VADDR	0xffff5000

extern start_info_t *avz_start_info;

shared_info_t *avz_map_shared_info(unsigned long pa);

/* Atomically write a string to the UART console */
int avz_printk(char *buffer);

/* Yield to another realtime ME */
int avz_sched_yield(void);

/* Ask avz to program a new deadline based on a delta expressed in ns (from now). */
int avz_sched_deadline(u64 delta_ns);

/* Sleep the RT-ME for <delta_ns> ns */
int avz_sched_sleep_ns(u64 delta_ns);
int avz_sched_sleep_us(u64 delta_us);
int avz_sched_sleep_ms(u64 delta_ms);

int avz_dump_page(unsigned int pfn);
int avz_dump_logbool(void);

void avz_ME_unpause(domid_t domain_id, uint32_t store_mfn);
void avz_ME_pause(domid_t domain_id);

#endif /* __HYPERVISOR_H__ */
