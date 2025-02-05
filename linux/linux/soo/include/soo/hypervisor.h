/*
 * Copyright (C) 2016-2025 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/cpumask.h>

void avz_printch(char c);
void avz_printstr(char *s);
 
#if defined(CONFIG_SOO)

void avz_get_shared(void);
void avz_gnttab(gnttab_op_t *op);

#endif

#endif /* __HYPERVISOR_H__ */
