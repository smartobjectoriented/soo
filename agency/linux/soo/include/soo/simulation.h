/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef SIMULATION_H
#define SIMULATION_H

#include <linux/list.h>

#include <soo/sooenv.h>

#include <soo/soolink/soolink.h>

#define BUFFER_SIZE 16*1024


/* SOO Smart Object topology (topo) management */

/**
 *  Simulation environment
 */
struct soo_simul_env {
	sl_desc_t *sl_desc;

	unsigned char buffer[BUFFER_SIZE];
	unsigned int recv_count;

	/* List of topo_node entries */
	struct list_head topo_links;
};

typedef struct {
	soo_env_t *node;

	struct list_head link;

} topo_node_entry_t;

void node_link(soo_env_t *soo, soo_env_t *target);
void node_unlink(soo_env_t *soo, soo_env_t *soo_target);

#endif /* SIMULATION_H */
