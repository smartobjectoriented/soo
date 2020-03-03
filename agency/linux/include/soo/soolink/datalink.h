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

#ifndef DATALINK_H
#define DATALINK_H

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/plugin.h>

/**
 * This is the descriptor of a protocol that can be used by Datalink.
 */
typedef struct {

	/* Function to inform if the sl_desc channel is ready for sending. */
	bool (*ready_to_send)(sl_desc_t *sl_desc);

	/* Function to be called when sending data */
	int (*xmit_callback)(sl_desc_t *sl_desc, void *packet, size_t size, bool completed);

	/* Function to be called when receiving data */
	void (*rx_callback)(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size);

	/* Function to be called when requesting to send data */
	int (*request_xmit_callback)(sl_desc_t *sl_desc);

} datalink_proto_desc_t;

void datalink_register_protocol(datalink_proto_t proto, datalink_proto_desc_t *proto_desc);
int datalink_request_xmit(sl_desc_t *sl_desc);
int datalink_xmit(sl_desc_t *sl_desc, void *packet, size_t size, bool completed);
void datalink_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size);

void datalink_init(void);

#endif /* DATALINK_H */
