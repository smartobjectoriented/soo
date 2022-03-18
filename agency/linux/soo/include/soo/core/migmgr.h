/*
 * Copyright (C) 2016-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef MIGMGR_H
#define MIGMGR_H

#include <linux/types.h>

bool initialize_migration(uint32_t slotID);
void finalize_migration(uint32_t slotID);

void write_snapshot(uint32_t slotID, void *buffer);

void copy_ME_snapshot_to_user(void *ME_snapshot, void *user_addr, uint32_t size);
int read_snapshot(uint32_t slotID, void **ME_buffer);


#endif /* MIGMGR_H */


