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

#include <soo/uapi/avz.h>

#include <soo/uapi/physdev.h>

#include <linux/cpumask.h>

extern bool __domcall_in_progress;

void avz_dump_page(unsigned int pfn);
void avz_dump_logbool(void);

void avz_ME_unpause(domid_t domID, addr_t vbstore_pfn);
void avz_ME_pause(domid_t domID);

void domcall(int cmd, void *arg);
void avz_linux_callback(void);

void avz_send_IPI(int ipinr, long cpu_mask);

#endif /* __HYPERVISOR_H__ */
