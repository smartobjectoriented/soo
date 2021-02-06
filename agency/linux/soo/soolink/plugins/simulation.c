/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

/* 
 * Plugin used for the network simulation for debugging and validation purposes.
 */

#include <linux/init.h>
#include <linux/if_ether.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/plugin.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/uapi/debug.h>

#include <soo/uapi/console.h>
#include <soo/evtchn.h>

#include <soo/debug/dbgvar.h>

#define PACKET_LENGTH_MAX 	2048

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

typedef struct {
	uint8_t *data;
	size_t size;
	__be16 proto;
	uint8_t *mac_addr;
} sim_packet_t;

struct soo_plugin_sim_env {

	bool plugin_ready;

	uint8_t mac[ETH_ALEN];

	medium_rx_back_ring_t rx_ring;

	plugin_desc_t simulation_desc;
};

#define current_soo_plugin_sim     ((struct soo_plugin_sim_env *) current_soo->soo_plugin->priv)

/*
 * Low-level function for sending to a smart object according to the MAC address of the destination.
 */
static bool sendto(soo_env_t *soo, void *args) {
	medium_rx_t *medium_rx, *rsp;
	bool broadcast, peer;

	medium_rx = (medium_rx_t *) args;

	/* <broadcast> is true if the MAC address is set to 00:00:00:00:00:00 */
	/* <peer> is true if the Smart Object corresponds to the destination MAC address */
	broadcast = !memcmp(medium_rx->mac_src, broadcast_addr, ETH_ALEN);
	peer = !memcmp(medium_rx->mac_src, soo->soo_plugin->sim->mac, ETH_ALEN);

	if (broadcast || peer) {

		soo_log("[soo:soolink:plugin:tx] ");
		if (broadcast)
			soo_log("(broadcast)");
		else
			soo_log("(peer)");

		soo_log(" sending to %s (%d bytes)\n", soo->name, medium_rx->size);

		while (RING_RSP_FULL(&soo->soo_plugin->sim->rx_ring))
			schedule();

		rsp = medium_rx_new_ring_response(&soo->soo_plugin->sim->rx_ring);

		rsp->plugin_desc = &soo->soo_plugin->sim->simulation_desc;
		rsp->req_type = medium_rx->req_type;

		rsp->size = medium_rx->size;

		/* The data will be freed by the rx threaded function of the plugin block. */
		rsp->data = kzalloc(rsp->size, GFP_KERNEL);
		BUG_ON(!rsp->data);

		memcpy(rsp->data, medium_rx->data, rsp->size);

		/* Update our MAC address in the packet, so that the receiver get the packet from us :-) */
		rsp->mac_src = kzalloc(ETH_ALEN, GFP_KERNEL);
		BUG_ON(!rsp->mac_src);

		memcpy(rsp->mac_src, current_soo_plugin_sim->mac, ETH_ALEN);

		medium_rx_ring_response_ready(&soo->soo_plugin->sim->rx_ring);

		complete(&soo->soo_plugin->rx_event);

		if (peer)
			return false;
	}

	return true;
}

/*
 * Transmit on the simulated interface.
 * At this point, we assume that the interface is up and available.
 */
void plugin_simulation_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	medium_rx_t medium_rx;
	const uint8_t *dest;

	soo_log("[soo:soolink:plugin:tx] transmitting to ");
	soo_log_printlnUID(&sl_desc->agencyUID_to);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&sl_desc->agencyUID_to);

	/* Maybe not yet known as a neighbour, while the other peer knows us. */
	if (dest == NULL)
		return ;

	soo_log("[soo:soolink:plugin:tx] MAC addr: ");
	soo_log_buffer((void *) dest, ETH_ALEN);
	soo_log("\n");

	medium_rx.req_type = sl_desc->req_type;

	medium_rx.mac_src = (uint8_t *) dest;

	medium_rx.data = data;
	medium_rx.size = size;

	/* Now proceed with sending the packet over the simulated network interface */
	iterate_on_other_soo(sendto, &medium_rx);

}

/**
 * This function must be executed in the non-RT domain.
 */
int plugin_simulation_init(void) {

	lprintk("(%s) SOOlink simulation plugin initializing ...\n", current_soo->name);

	current_soo->soo_plugin->priv = (struct soo_plugin_sim_env *) kzalloc(sizeof(struct soo_plugin_sim_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_plugin->priv);

	current_soo_plugin_sim->simulation_desc.tx_callback = plugin_simulation_tx;
	current_soo_plugin_sim->simulation_desc.if_type = SL_IF_SIMULATION;

	transceiver_plugin_register(&current_soo_plugin_sim->simulation_desc);

	current_soo_plugin_sim->plugin_ready = true;

	/* Assign a (virtual) MAC address */
	memset(current_soo_plugin_sim->mac, 0, ETH_ALEN);
	current_soo_plugin_sim->mac[4] = 0x12;
	current_soo_plugin_sim->mac[5] = current_soo->id;

	lprintk("--> On SOO %s, UID: ", current_soo->name);
	lprintk_buffer(&current_soo->agencyUID, 5);
	lprintk(" / assigning MAC address :");
	lprintk_buffer(current_soo_plugin_priv->mac, ETH_ALEN);
	lprintk("\n");

	/* Initialize the backend shared ring for RX packet */
	BACK_RING_INIT(&current_soo_plugin_sim->rx_ring, current_soo_plugin->rx_ring.sring, 32 * PAGE_SIZE);

	return 0;
}


