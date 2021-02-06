/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef ASM_MEMORY_H
#define ASM_MEMORY_H

#include <generated/autoconf.h>

#ifdef CONFIG_SO3VIRT

/*
 * Normally, VMALLOC_END (Linux) finishes at 0xf8000000, and some I/O like UARTs might be mapped.
 * So we preserve re-adjusting pfns in these regions (below the hypervisor).
 */
#define	HYPERVISOR_VIRT_ADDR		0xff000000
#define HYPERVISOR_VIRT_ADDR_DBG	0xf8000000
#define HYPERVISOR_VIRT_SIZE		0x00c00000

#define HYPERVISOR_VBSTORE_VADDR	0xffe01000

#ifdef __ASSEMBLY__
.extern avz_guest_phys_offset
#else
#include <types.h>
#include <list.h>
extern uint32_t avz_guest_phys_offset;

#endif /* __ASSEMBLY__ */

#define CONFIG_RAM_BASE		(avz_guest_phys_offset)

#endif /* CONFIG_SO3VIRT */
#ifndef __ASSEMBLY__
#include <types.h>

extern uint32_t *__sys_l1pgtable;

#endif /* __ASSEMBLY__ */

#endif /* ASM_MEMORY_H */
