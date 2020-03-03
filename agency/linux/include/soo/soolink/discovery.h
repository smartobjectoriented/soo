/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <linux/types.h>
#include <linux/list.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/plugin.h>

#include <soo/uapi/soo.h>

#define SOOLINK_PRESENCE_TICK_MAX	10

typedef struct {

	agencyUID_t	agencyUID;
	uint8_t		name[SOO_NAME_SIZE];
	plugin_desc_t	*plugin;
	uint32_t	presence_tick;

	bool		present;

	/* List of neighbours */
	struct list_head list;

} neighbour_desc_t;

typedef struct {
	uint8_t	agencyUID[SOO_AGENCY_UID_SIZE];
	uint8_t name[SOO_NAME_SIZE];
} iamasoo_pkt_t;

typedef struct {

	/* When a neighbour appears */
	void (*add_neighbour_callback)(neighbour_desc_t *neighbour);

	/* When a neighbour disappears */
	void (*remove_neighbour_callback)(neighbour_desc_t *neighbour);

	struct list_head list;

} discovery_listener_t;

/* Get the list of current neighbours. Entries of this list are neighbour_desc_t entries */
int discovery_get_neighbours(struct list_head *new_list);
void discovery_clear_neighbour_list(struct list_head *list);

void discovery_rx(plugin_desc_t *plugin_desc, void *data, size_t size);

void discovery_listener_register(discovery_listener_t *listener);

void neighbour_list_protection(bool protect);

void discovery_init(void);
void discovery_start(void);

void discovery_enable(void);
void discovery_disable(void);

void discovery_dump_neighbours(void);

#endif /* DISCOVERY_H */
