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

#ifndef PLUGIN_H
#define PLUGIN_H

#include <linux/list.h>
#include <linux/if_ether.h>

#include <soo/ring.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/netsimul.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/plugin/common.h>

typedef struct {

	/* To help for a list of available plugin */
	struct list_head list;

	/* Associated interface type for this plugin */
	if_type_t if_type;

	/* Function to be called when sending data out */
	void (*tx_callback)(sl_desc_t *sl_desc, void *data, size_t size);

} plugin_desc_t;

typedef struct {
	plugin_desc_t *plugin_desc;
	req_type_t req_type;
	void *data;
	size_t size;
	uint8_t *mac_src;
} medium_rx_t;

DEFINE_RING_TYPES(medium_rx, medium_rx_t, medium_rx_t);


struct soo_plugin_env {
	struct list_head remote_soo_list;

	spinlock_t list_lock;
	struct list_head plugin_list;

	/* Rx part */
	medium_rx_front_ring_t rx_ring;

	struct completion rx_event;

	/* Specific plugin related data */
	void *priv;

};

#define current_soo_plugin_priv     (current_soo->soo_plugin->priv)

void transceiver_plugin_init(void);

void transceiver_plugin_register(plugin_desc_t *plugin_desc);
void transceiver_plugins_enable(void);

void plugin_tx(sl_desc_t *sl_desc, void *data, size_t size);

uint8_t *get_mac_addr(agencyUID_t *agencyUID);

void attach_agencyUID(agencyUID_t *agencyUID, uint8_t *mac_src);
void detach_agencyUID(agencyUID_t *agencyUID);

req_type_t get_sl_req_type_from_protocol(uint16_t protocol);
uint16_t get_protocol_from_sl_req_type(req_type_t req_type);

/* Supported plugins */
int plugin_simulation_init(void);
int plugin_ethernet_init(void);


#endif /* PLUGIN_H */
