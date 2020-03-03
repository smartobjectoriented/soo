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

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/plugin/common.h>

typedef struct {

	/* To help for a list of available plugin */
	struct list_head list;

	/* Associated interface type for this plugin */
	if_type_t if_type;

	/* Function to be called when sending data out */
	void (*tx_callback)(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags);


} plugin_desc_t;

void transceiver_plugin_init(void);

void transceiver_plugin_register(plugin_desc_t *plugin_desc);

void plugin_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags);
void plugin_rx(plugin_desc_t *plugin_desc, agencyUID_t *agencyUID_from, req_type_t req_type, void *data, size_t size);

extern unsigned char broadcast_addr[ETH_ALEN];

req_type_t get_sl_req_type_from_protocol(uint16_t protocol);
uint16_t get_protocol_from_sl_req_type(req_type_t req_type);

#endif /* PLUGIN_H */
