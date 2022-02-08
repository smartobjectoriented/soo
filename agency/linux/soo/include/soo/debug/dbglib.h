/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef DBGLIB_H
#define DBGLIB_H

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

uint32_t dbglib_reserve_free_slot(void);

int dbglib_collect_sample(uint32_t slot, s64 sample);
s64 dbglib_collect_sample_and_get_mean(uint32_t slot, s64 sample);
void dbglib_collect_sample_and_show_mean(char *pre, uint32_t slot, s64 sample, char *post);

void dbglib_init(void);

#endif /* DBGLIB_H */
