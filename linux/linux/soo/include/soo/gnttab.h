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

#ifndef GNTTAB_H
#define GNTTAB_H

#include <soo/grant_table.h>

#include <linux/version.h>

#define NR_GRANT_FRAMES 4
#define NR_GRANT_ENTRIES (NR_GRANT_FRAMES * PAGE_SIZE / sizeof(grant_entry_t))

static inline void gnttab_set_map_op(struct gnttab_map_grant_ref *map, phys_addr_t addr, uint32_t flags, grant_ref_t ref, domid_t domid, unsigned int offset, unsigned int size)
{
	map->host_addr = addr;
	map->flags = flags;
	map->ref = ref;
	map->dom = domid;

	map->handle = 0;

	map->dev_bus_addr = 0;
	map->status = 0;

	map->size = size;   /* By default... */
	map->offset = offset;

}

static inline void gnttab_set_unmap_op(struct gnttab_unmap_grant_ref *unmap, phys_addr_t addr, uint32_t flags, grant_handle_t handle)
{
	unmap->host_addr = addr;
	unmap->dev_bus_addr = 0;
	unmap->flags = flags;
	unmap->ref = 0;

	unmap->handle = handle;

	unmap->dev_bus_addr = 0;
	unmap->status = 0;

	unmap->size = PAGE_SIZE;   /* By default... */
	unmap->offset = 0;

}

void grant_table_op(unsigned int cmd, void *uop, unsigned int count);

extern void gnttab_map(struct gnttab_map_grant_ref *op);
extern void gnttab_unmap(struct gnttab_unmap_grant_ref *op);
extern void gnttab_copy(struct gnttab_copy *op);
extern void gnttab_map_with_copy(struct gnttab_map_grant_ref *op);
extern void gnttab_unmap_with_copy(struct gnttab_unmap_grant_ref *op);

void postmig_gnttab_update(void);


#endif /* GNTTAB_H */
