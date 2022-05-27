
/*
 * Copyright (C) 2016-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <soo/uapi/avz.h>

extern volatile avz_shared_t *avz_shared;

extern void hypercall_trampoline(int hcall, long a0, long a2, long a3, long a4);

#define AVZ_shared ((smp_processor_id() == 1) ? (avz_shared)->subdomain_shared : avz_shared)

#define AVZ_primary_shared ((avz_shared_t *) avz_shared)
