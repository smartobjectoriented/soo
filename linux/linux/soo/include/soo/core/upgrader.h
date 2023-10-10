/*
 * Copyright (C) 2019 David Truan <david.truan@heig-vd.ch>
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

#ifndef UPGRADER_H
#define UPGRADER_H

#include <linux/fs.h>

void get_upgrade_image(uint32_t *size, uint32_t *slotID);

void store_versions(upgrade_versions_args_t *versions);

int agency_upgrade_mmap(struct file *filp, struct vm_area_struct *vma);

void upg_store(uint32_t buffer_pfn, uint32_t buffer_size, unsigned int ME_slotID);

#endif /* UPGRADER_H */
