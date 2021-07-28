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

int ioctl_set_personality(unsigned long arg);
int ioctl_get_personality(unsigned long arg);

int ioctl_initialize_migration(unsigned long arg);
int ioctl_finalize_migration(unsigned long arg);

int ioctl_write_snapshot(unsigned long arg);

void ioctl_get_ME_snapshot(unsigned long arg);
int ioctl_read_snapshot(unsigned long arg);


#endif /* MIGMGR_H */


