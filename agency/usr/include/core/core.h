/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef CORE_H
#define CORE_H

#include <stddef.h>

#include <soo/uapi/soo.h>

#define AGENCY_CORE_VERSION "2022.3"

/* Cycle period express in ms */
#define AG_CYCLE_PERIOD		100

#define SOO_CORE_DEVICE		"/dev/soo/core"

extern int fd_core;
extern int fd_dcm;

/* Arguments of the agency application */
extern bool opt_noinject;
extern bool opt_nosend;

/* To indicate f the main agency cycle loop is interrupted */
extern bool ag_cycle_interrupted;

int initialize_migration(unsigned int ME_slotID);

int get_ME_free_slot(size_t ME_size);

bool get_ME_id(uint32_t slotID, ME_id_t *ME_id);

void read_ME_snapshot(unsigned int slotID, void **buffer, uint32_t *buffer_size);
void write_ME_snapshot(unsigned int slotID, unsigned char *ME_buffer);

void *prepare_ME_slot(unsigned int slotID);
int finalize_migration(unsigned int slotID);

void main_loop(int cycle_period);

void migration_init(void);

#endif /* CORE_H */
