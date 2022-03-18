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

#include <soo/simulation.h>

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

typedef struct {
	uint8_t mac[ETH_ALEN];
	plugin_desc_t plugin_sim_desc;

	/* Simulated RX packet to be queued. */
	medium_rx_t __medium_rx;

	struct completion rx_event, rx_completed_event;
	struct mutex rx_lock;

} soo_plugin_sim_t;

/* SOO Smart Object topology (topo) management */

/**
 * Link a SOO (node) with another node.
 *
 * @param soo The smart object which becomes visible to the <soo_target>
 * @param soo_target
 */
void node_link(soo_env_t *soo, soo_env_t *target) {
	topo_node_entry_t *topo_node_entry;

	/* Create a new soo_topo entry */
	topo_node_entry = kzalloc(sizeof(topo_node_entry_t), GFP_KERNEL);
	BUG_ON(!topo_node_entry);

	topo_node_entry->node = soo;

	list_add_tail(&topo_node_entry->link, &target->soo_simul->topo_links);
}

topo_node_entry_t *find_node(soo_env_t *soo, soo_env_t *target) {
	topo_node_entry_t *topo_node_entry;

	list_for_each_entry(topo_node_entry, &target->soo_simul->topo_links, link)
		if (topo_node_entry->node == soo)
			return topo_node_entry;

	return NULL;
}

/**
 * Perform a call to <fn> on each known instance of SOO.
 * @param fn Function callback to execute
 * @param args Reference to args as passed to the iterator.
 */
void iterate_on_topo_nodes(soo_iterator_t fn, void *args) {
	soo_env_t *soo;
	bool cont;
	topo_node_entry_t *topo_node_entry;

	list_for_each_entry(topo_node_entry, &current_soo->soo_simul->topo_links, link)
	{
		soo = topo_node_entry->node;

		cont = fn(soo, args);

		if (!cont)
			break;
	}
}

/**
 * Unlink a SOO (node) from a target SOO.
 *
 * @param soo
 * @param soo_target
 */
void node_unlink(soo_env_t *soo, soo_env_t *soo_target) {
	topo_node_entry_t *topo_node_entry;

	topo_node_entry = find_node(soo, soo_target);
	BUG_ON(!topo_node_entry);

	list_del(&topo_node_entry->link);

	kfree(topo_node_entry);
}

/**
 * This thread simulates asynchronous activity at the receiver side.
 * Normally, the network driver executes the rx in a workqueue.
 */
static int plugin_sim_rx_fn(void *args) {
	soo_plugin_sim_t *soo_plugin_sim;

	soo_plugin_sim = container_of(current_soo_plugin->__intf[SL_IF_SIM], soo_plugin_sim_t, plugin_sim_desc);

	/* Wait on a complete from the sending context */
	while (true) {

		wait_for_completion(&soo_plugin_sim->rx_event);

		plugin_rx(&soo_plugin_sim->plugin_sim_desc, soo_plugin_sim->__medium_rx.req_type, soo_plugin_sim->__medium_rx.mac_src,
			  soo_plugin_sim->__medium_rx.data, soo_plugin_sim->__medium_rx.size);

		complete(&soo_plugin_sim->rx_completed_event);
	}

	return 0;
}

/*
 * Low-level function for sending to a smart object according to the MAC address of the destination.
 */
static bool sendto(soo_env_t *soo, void *args) {
	medium_rx_t *medium_rx;
	soo_plugin_sim_t *soo_plugin_sim, *soo_plugin_sim_dst;
	bool broadcast;

	soo_plugin_sim = container_of(current_soo_plugin->__intf[SL_IF_SIM], soo_plugin_sim_t, plugin_sim_desc);
	soo_plugin_sim_dst = container_of(soo->soo_plugin->__intf[SL_IF_SIM], soo_plugin_sim_t, plugin_sim_desc);

	medium_rx = (medium_rx_t *) args;

	broadcast = (memcmp(medium_rx->mac_dst, broadcast_addr, ETH_ALEN) ? false : true);

	/* For a peer transmission, are we the target SOO ? */
	if (!broadcast && memcmp(medium_rx->mac_dst, soo_plugin_sim_dst->mac, ETH_ALEN))
		return true;

	soo_log("[soo:soolink:plugin:tx] %s", (broadcast ? "(broadcast)" : "(peer)"));
	soo_log("  sending to %s (%d bytes)\n", soo->name, medium_rx->size);

	/* Several SOOs could send to this destination...*/
	mutex_lock(&soo_plugin_sim_dst->rx_lock);

	/* We pass the medium_rx packet to the target thread */
	soo_plugin_sim_dst->__medium_rx.req_type = medium_rx->req_type;
	soo_plugin_sim_dst->__medium_rx.mac_src = medium_rx->mac_src;
	soo_plugin_sim_dst->__medium_rx.data = medium_rx->data;
	soo_plugin_sim_dst->__medium_rx.size = medium_rx->size;

	/* Simulate the receival of a packet on the target SOO. */
	complete(&soo_plugin_sim_dst->rx_event);

	/* Synchronous wait until the target thread was able to queue the RX packet. */
	wait_for_completion(&soo_plugin_sim_dst->rx_completed_event);

	mutex_unlock(&soo_plugin_sim_dst->rx_lock);

	/* If the packet is not broadcast, we abort the iterator on other SOOs. */
	if (!broadcast)
		return false;
	else
		return true;
}

/*
 * Transmit on the simulated interface.
 * At this point, we assume that the interface is up and available.
 */
void plugin_simulation_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	medium_rx_t medium_rx;
	soo_plugin_sim_t *soo_plugin_sim;

	soo_log("[soo:soolink:plugin:tx] transmitting to ");
	soo_log_printlnUID(sl_desc->agencyUID_to);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!sl_desc->agencyUID_to)
		medium_rx.mac_dst = broadcast_addr;
	else
		medium_rx.mac_dst = get_mac_addr(sl_desc->agencyUID_to);

	/* Maybe not yet known as a neighbour, while the other peer knows us. */
	if (medium_rx.mac_dst == NULL)
		return ;

	soo_log("[soo:soolink:plugin:tx] MAC addr: ");
	soo_log_buffer((void *) medium_rx.mac_dst, ETH_ALEN);
	soo_log("\n");

	medium_rx.req_type = sl_desc->req_type;

	soo_plugin_sim = container_of(current_soo_plugin->__intf[SL_IF_SIM], soo_plugin_sim_t, plugin_sim_desc);

	medium_rx.mac_src = soo_plugin_sim->mac;

	medium_rx.data = data;
	medium_rx.size = size;

	/* Now proceed with sending the packet over the simulated network interface */
	iterate_on_topo_nodes(sendto, &medium_rx);
}

/**
 * This function must be executed in the non-RT domain.
 */
void plugin_simulation_init(void) {
	soo_plugin_sim_t *soo_plugin_sim;
	struct task_struct *__ts;

	lprintk("(%s) SOOlink simulation plugin initializing ...\n", current_soo->name);

	soo_plugin_sim = (soo_plugin_sim_t *) kzalloc(sizeof(soo_plugin_sim_t), GFP_KERNEL);
	BUG_ON(!soo_plugin_sim);

	soo_plugin_sim->plugin_sim_desc.tx_callback = plugin_simulation_tx;
	soo_plugin_sim->plugin_sim_desc.if_type = SL_IF_SIM;

	transceiver_plugin_register(&soo_plugin_sim->plugin_sim_desc);

	init_completion(&soo_plugin_sim->rx_event);
	init_completion(&soo_plugin_sim->rx_completed_event);

	mutex_init(&soo_plugin_sim->rx_lock);

	__ts = kthread_create(plugin_sim_rx_fn, NULL, "plugin_sim_rx");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);

	wake_up_process(__ts);

	/* Assign a (virtual) MAC address */
	memset(soo_plugin_sim->mac, 0, ETH_ALEN);
	soo_plugin_sim->mac[4] = 0x12;
	soo_plugin_sim->mac[5] = current_soo->id;

	soo_log("[soo:soolink:plugin] On SOO %s, UID: ", current_soo->name);
	soo_log_printUID(current_soo->agencyUID);

	soo_log(" / assigning MAC address :");
	soo_log_buffer(soo_plugin_sim->mac, ETH_ALEN);
	soo_log("\n");
}


