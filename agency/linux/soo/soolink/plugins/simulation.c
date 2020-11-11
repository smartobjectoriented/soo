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
#include <linux/kthread.h>

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
	uint8_t data[PACKET_LENGTH_MAX];
	size_t size;
	__be16 proto;
	uint8_t *mac_addr;
} sim_packet_t;

struct soo_plugin_sim_env {

	bool plugin_ready;

	uint8_t mac[ETH_ALEN];
	struct completion rx_event;

	sim_packet_t *rx_packet;
	struct mutex rx_lock;

	plugin_desc_t simulation_desc;
};

/*
 * Low-level function for sending to a smart object according to the MAC address of the destination.
 */
static bool sendto(soo_env_t *soo, void *args) {
	sim_packet_t *sim_packet;

	sim_packet = (sim_packet_t *) args;

	if (!memcmp(sim_packet->mac_addr, broadcast_addr, ETH_ALEN) || !memcmp(sim_packet->mac_addr, soo->soo_plugin->sim->mac, ETH_ALEN)) {

		soo_log("[soo:soolink:plugin] ");
		if (!memcmp(sim_packet->mac_addr, broadcast_addr, ETH_ALEN))
			soo_log("(broadcast)");
		else
			soo_log("(peer)");
		soo_log(" sending to %s (%d bytes)\n", soo->name, sim_packet->size);

		/* Update our MAC address in the packet, so that the receiver get the packet from us :-) */
		sim_packet->mac_addr = current_soo_plugin_sim->mac;

		/* Wait until the receiver has processed a previous packet eventually. */

		mutex_lock(&soo->soo_plugin->sim->rx_lock);

		soo->soo_plugin->sim->rx_packet = sim_packet;

		complete(&soo->soo_plugin->sim->rx_event);

		if (!memcmp(sim_packet->mac_addr, soo->soo_plugin->sim->mac, ETH_ALEN))
			return false;
	}

	return true;
}

/*
 * Transmit on the simulated interface.
 * At this point, we assume that the interface is up and available.
 */
void plugin_simulation_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	sim_packet_t *sim_packet;
	const uint8_t *dest;

	soo_log("[soo:soolink:plugin] transmitting to ");
	soo_log_printlnUID(&sl_desc->agencyUID_to);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&sl_desc->agencyUID_to);

	/* Maybe not yet known as a neighbour, while the other peer knows us. */
	if (dest == NULL)
		return ;

	soo_log("[soo:soolink:plugin] MAC addr: ");
	soo_log_buffer((void *) dest, ETH_ALEN);
	soo_log("\n");

	sim_packet = kzalloc(sizeof(sim_packet_t), GFP_KERNEL);
	BUG_ON(!sim_packet);

	sim_packet->proto = get_protocol_from_sl_req_type(sl_desc->req_type);

	sim_packet->mac_addr = (uint8_t *) dest;

	memcpy(sim_packet->data, data, size);
	sim_packet->size = size;

	/* Now proceed with sending the packet over the simulated network interface */
	iterate_on_other_soo(sendto, sim_packet);
}

int plugin_simulation_rx_fn(void *args) {
	req_type_t req_type;

	while (true) {

		/* Wait for a packet */
		wait_for_completion(&current_soo_plugin_sim->rx_event);

		soo_log("[soo:soolink:plugin] Got a packet (%d bytes)\n", current_soo_plugin_sim->rx_packet->size);

		req_type = get_sl_req_type_from_protocol(current_soo_plugin_sim->rx_packet->proto);

		plugin_rx(&current_soo_plugin_sim->simulation_desc, req_type, current_soo_plugin_sim->rx_packet->data, current_soo_plugin_sim->rx_packet->size, current_soo_plugin_sim->rx_packet->mac_addr);

		kfree(current_soo_plugin_sim->rx_packet);

		/* Inform the sender that we are ready for processing another packet */
		mutex_unlock(&current_soo_plugin_sim->rx_lock);

	}

	return 0;
}

/**
 * This function must be executed in the non-RT domain.
 */
int plugin_simulation_init(void) {
	struct task_struct *__ts;

	lprintk("(%s) SOOlink simulation plugin initializing ...\n", current_soo->name);

	current_soo->soo_plugin->sim = kzalloc(sizeof(struct soo_plugin_sim_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_plugin->sim);

	current_soo_plugin_sim->simulation_desc.tx_callback = plugin_simulation_tx;
	current_soo_plugin_sim->simulation_desc.if_type = SL_IF_SIMULATION;

	mutex_init(&current_soo->soo_plugin->sim->rx_lock);

	transceiver_plugin_register(&current_soo_plugin_sim->simulation_desc);

	current_soo_plugin_sim->plugin_ready = true;

	/* Assign a (virtual) MAC address */
	memset(current_soo_plugin_sim->mac, 0, ETH_ALEN);
	current_soo_plugin_sim->mac[4] = 0x12;
	current_soo_plugin_sim->mac[5] = current_soo->id;

	lprintk("--> On SOO %s, assigning MAC address :", current_soo->name);
	lprintk_buffer(current_soo_plugin_sim->mac, ETH_ALEN);
	lprintk("\n");

	init_completion(&current_soo_plugin_sim->rx_event);

	__ts = kthread_create(plugin_simulation_rx_fn, NULL, "plugin_simulation_rx");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);

	wake_up_process(__ts);

	return 0;
}


