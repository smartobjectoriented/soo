/*
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/types.h>
#include <linux/spinlock.h>

#include <soo/soolink/discovery.h>
#include <soo/soolink/sender.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

static bool discovery_enabled = false;

static struct list_head neighbour_list;
static rtdm_task_t rt_watch_loop_task, rt_soo_stream_task;
iamasoo_pkt_t iamasoo_beacon_pkt;

static struct list_head discovery_listener_list;
static bool __neighbour_list_protected;

/* Used to maintain new discovered neighbours if the main list is protected. */
static struct list_head discovery_pending_add_list;

static struct list_head neigh_blacklist;

static spinlock_t discovery_listener_lock;

static sl_desc_t *discovery_sl_desc;

/**
 * Call the add callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_add_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_listener;

	list_for_each(cur, &discovery_listener_list) {
		cur_listener = list_entry(cur, discovery_listener_t, list);

		if (cur_listener->add_neighbour_callback)
			cur_listener->add_neighbour_callback(neighbour);
	}
}

/**
 * Call the remove callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_remove_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_neighbour;

	list_for_each(cur, &discovery_listener_list) {
		cur_neighbour = list_entry(cur, discovery_listener_t, list);

		if (cur_neighbour->remove_neighbour_callback)
			cur_neighbour->remove_neighbour_callback(neighbour);
	}
}

/* Used to configure the neighbourhood. The bitmap indicates which
 * neighbours must be considered in the list of neighbours (the bitmap
 * position corresponds to the n-th item of the list).
 * Example: neigh_bitmap = 4 => only the third neighbours has to be considered, neigh_bitmap = 3,
 * only the first and second neighbours, etc.
 * If neigh_bitmap == -1 (0xffffffff) => all neighbours are considered.
 * The neighbourhood configuration must be performed once the neighbourhood is stable, typically by means
 * of the application agency_core -n <list_of_pos> (see agency_core application).
 * The neighbours are simply blacklisted.
 * This facility is mainly used for debugging purposes.
 */
void configure_neighbitmap(uint32_t neigh_bitmap) {
	uint32_t i;
	struct list_head *cur_neigh, *tmp;
	neighbour_desc_t *cur_neighbour;

	spin_lock(&discovery_listener_lock);

	cur_neigh = &neighbour_list;
	cur_neigh = cur_neigh->next;

	i = 0;
	list_for_each_safe(cur_neigh, tmp, &neighbour_list) {

		if (!(neigh_bitmap & (1 << i))) {
			/* To be blacklisted */

			/* Detach it */
			cur_neigh->prev->next = cur_neigh->next;
			cur_neigh->next->prev = cur_neigh->prev;

			cur_neighbour = list_entry(cur_neigh, neighbour_desc_t, list);

			lprintk("%s: blacklist neighbour: ", __func__); lprintk_buffer(&cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			/* And attach to the blacklist */
			list_add_tail(cur_neigh, &neigh_blacklist);

			callbacks_remove_neighbour(cur_neighbour);
		}

		i++;
	}

	spin_unlock(&discovery_listener_lock);
}

/*
 * Reset the blacklisted neighbours bitmap.
 * Neighbours will re-appear as soon as their Iamasoo beacon is received.
 */
void reset_neighbitmap(void) {

	INIT_LIST_HEAD(&neigh_blacklist);
}

/*
 * Add the neighbour in the list definitively.
 * The lock on the list must be taken.
 */
static void __add_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	int position_prev, position_cur = 0;
	neighbour_desc_t *cur_neighbour, *prev_neighbour;

	/* If the list is empty, add the neighbour to it */
	if (unlikely(list_empty(&neighbour_list)))
		list_add_tail(&neighbour->list, &neighbour_list);
	/* cur_neighbour is currently the last element of the list */
	else {
		list_for_each(cur, &neighbour_list) {
			position_prev = position_cur;
			prev_neighbour = cur_neighbour;

			cur_neighbour = list_entry(cur, neighbour_desc_t, list);
			position_cur = memcmp(&cur_neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			if (position_cur > 0) {
				/* Head of the list */
				if (position_prev == 0)
					list_add(&neighbour->list, &neighbour_list);
				/* The neighbour must be inserted between two neighbours in the list  */
				else if (position_prev < 0)
					list_add(&neighbour->list, &prev_neighbour->list);
			}
		}

		/* Tail of the list */
		if (position_cur < 0)
			list_add_tail(&neighbour->list, &neighbour_list);
	}

	/* Call the neighbour add callbacks */
	callbacks_add_neighbour(neighbour);
}


/*
 * Add a new neighbour in the list.
 * The lock on the list must be taken.
 */
static void add_neighbour(neighbour_desc_t *neighbour) {
	neighbour_desc_t *cur_neighbour;
	bool found = false;
	struct list_head *cur;

	/* If the list is protected, it will be added later during the unprotect operation. */
	if (__neighbour_list_protected) {

		/* Check if already known in the add pending list */
		list_for_each(cur, &discovery_pending_add_list) {
			cur_neighbour = list_entry(cur, neighbour_desc_t, list);
			if (!memcmp(&cur_neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE)) {
				found = true;
				break;
			}
		}
		if (!found)
			list_add(&neighbour->list, &discovery_pending_add_list);
	} else
		__add_neighbour(neighbour);
}

/**
 * Asynchronous callback function called when a Iamasoo packet is received.
 */
void discovery_rx(plugin_desc_t *plugin_desc, void *data, size_t size) {
	bool known_neighbour = false;
	struct list_head *cur;
	neighbour_desc_t *neighbour, *cur_neighbour;
	iamasoo_pkt_t *iamasoo_pkt = (iamasoo_pkt_t *) data;

	spin_lock(&discovery_listener_lock);

	/* Look for the neighbour in the list */
	list_for_each(cur, &neighbour_list) {
		cur_neighbour = list_entry(cur, neighbour_desc_t, list);
		if (!memcmp(&cur_neighbour->agencyUID, data, SOO_AGENCY_UID_SIZE)) {
			known_neighbour = true;
			break;
		}
	}

	/* Do not try to add a blacklisted neighbour */
	if (!known_neighbour) {
		list_for_each(cur, &neigh_blacklist) {
			cur_neighbour = list_entry(cur, neighbour_desc_t, list);
			if (!memcmp(&cur_neighbour->agencyUID, data, SOO_AGENCY_UID_SIZE)) {
				known_neighbour = true;
				break;
			}
		}
	}

	/* If the neighbour is not in the list... */
	if (!known_neighbour) {
		/* Add the neighbour in the list */
		neighbour = (neighbour_desc_t *) kmalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
		if (!neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		memcpy(&neighbour->agencyUID, iamasoo_pkt->agencyUID, SOO_AGENCY_UID_SIZE);
		memcpy(neighbour->name, iamasoo_pkt->name, SOO_NAME_SIZE);

		neighbour->presence_tick = 0;
		neighbour->present = true;
		neighbour->plugin = plugin_desc;

		DBG("Add the neighbour: %s - ", neighbour->name);
		DBG_BUFFER(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		add_neighbour(neighbour);

	} else {

		/* If the neighbour is already known */

		DBG("Reset the neighbour's presence_tick to 0: ");
		DBG_BUFFER(&cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		/* Update the name of the Smart Object (it can change) */
		memcpy(cur_neighbour->name, iamasoo_pkt->name, SOO_NAME_SIZE);

		/* Reset the neighbour's presence_tick to 0. */
		cur_neighbour->presence_tick = 0;
		cur_neighbour->present = true;  /* If disappeared in between */
	}

	spin_unlock(&discovery_listener_lock);
}

/**
 * Send a Iamasoo beacon.
 */
static void send_beacon(void) {
	/* If the agency UID has not been initialized yet, do not send any Iamasoo beacon */
	if (unlikely(!agencyUID_is_valid(get_my_agencyUID())))
		return ;

	/* Copy the agency UID and the name of this Smart Object into the beacon packet */
	memcpy(&iamasoo_beacon_pkt.agencyUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
	memset(iamasoo_beacon_pkt.name, 0, SOO_NAME_SIZE);
	devaccess_get_soo_name(iamasoo_beacon_pkt.name);

	/* Now send the Iamasoo beacon to the sender */
	DBG("%s: sending to: ", __func__);
	DBG_BUFFER(&discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

	sender_xmit(discovery_sl_desc, &iamasoo_beacon_pkt, sizeof(iamasoo_pkt_t), true);
}

/**
 * Infinite loop that checks if the neighbours are alive.
 */
static void iamasoo_task_fn(void *args) {
	struct list_head *cur, *tmp;
	neighbour_desc_t *neighbour;

	while (1) {
		rtdm_task_wait_period(NULL);

		if (!discovery_enabled)
			continue;

		/* Broadcast the beacon over all plugins */
		send_beacon();

		spin_lock(&discovery_listener_lock);

		/* Increment the presence_tick, over the neighbours */
		list_for_each(cur, &neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);
			neighbour->presence_tick++;
		}

		/* If the list is protected, we admin the possibility to have an entry which may disappear during
		 * the protected phase. However, it will stay in the list and will disappear once it will integrate
		 * the main list.
		 */

		/* Check if a neighbour is dead */
		list_for_each_safe(cur, tmp, &neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			/* If a neighbour is dead, delete it from the list */
			if (neighbour->presence_tick >= SOOLINK_PRESENCE_TICK_MAX) {

				if (__neighbour_list_protected)
					neighbour->present = false;  /* It will be removed later, during unprotect operation */
				else {
					/* Call the neighbour remove callbacks */
					callbacks_remove_neighbour(neighbour);

					DBG("Delete the neighbour: ");
					DBG_BUFFER(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					list_del(cur);
					kfree(neighbour);
				}
			}
		}

		spin_unlock(&discovery_listener_lock);
	}
}

/**
 * Make a copy of the active neighbour list. The elements are allocated on-the-fly.
 * If there is no memory available and/or if the allocation has failed, the function will enter into bug.
 */
int discovery_get_neighbours(struct list_head *new_list) {
	struct list_head *cur;
	neighbour_desc_t *neighbour, *new_neighbour;
	uint32_t count = 0;

	spin_lock(&discovery_listener_lock);

	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		/* Add the neighbour in the list */
		new_neighbour = (neighbour_desc_t *) kmalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
		if (!new_neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		/* Copy the attributes */
		memcpy(&new_neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		memcpy(new_neighbour->name, neighbour->name, SOO_NAME_SIZE);
		new_neighbour->plugin = neighbour->plugin;
		new_neighbour->presence_tick = neighbour->presence_tick;

		/* Add the new element to the output list */
		list_add_tail(&new_neighbour->list, new_list);

		count++;
	}

	spin_unlock(&discovery_listener_lock);

	return count;
}

/*
 * Enable the protection on the main neighbour list.
 * Used to prevent any changes during some operations.
 */
void neighbour_list_protection(bool protect) {
	struct list_head *cur, *tmp;
	neighbour_desc_t *neighbour;

	spin_lock(&discovery_listener_lock);

	BUG_ON(protect && __neighbour_list_protected);
	BUG_ON(!protect && !__neighbour_list_protected);

	/* Check for a need of synchronization with the pending list. */
	if (!protect) {

		/* Possible pending addings ? */
		list_for_each_safe(cur, tmp, &discovery_pending_add_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			/* Change of list */
			list_del(cur);
			__add_neighbour(neighbour);
		}

		/* We check if some neighbour disappeared in the meanwhile */
		list_for_each_safe(cur, tmp, &neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			if (!neighbour->present) {

				/* Call the neighbour remove callbacks */
				callbacks_remove_neighbour(neighbour);

				list_del(cur);
				kfree(neighbour);
			}
		}
	}

	__neighbour_list_protected = protect;

	spin_unlock(&discovery_listener_lock);
}


/**
 * Clear the neighbour list given as parameter. Its elements are freed.
 */
void discovery_clear_neighbour_list(struct list_head *list) {
	struct list_head *cur, *tmp;
	neighbour_desc_t *neighbour;

	spin_lock(&discovery_listener_lock);

	list_for_each_safe(cur, tmp, list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		list_del(cur);
	}
	spin_unlock(&discovery_listener_lock);
}

/**
 * Dump the active neighbour list.
 */
void discovery_dump_neighbours(void) {
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;

	spin_lock(&discovery_listener_lock);

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&neighbour_list)) {
		lprintk("No neighbour\n");
		spin_unlock(&discovery_listener_lock);
		return;
	}

	list_for_each(cur, &neighbour_list) {

		neighbour = list_entry(cur, neighbour_desc_t, list);

		lprintk("- Neighbour %d: %s - ", count+1, neighbour->name);
		lprintk_buffer(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		count++;
	}

	spin_unlock(&discovery_listener_lock);
}

/**
 * Register a Discovery listener.
 */
void discovery_listener_register(discovery_listener_t *listener) {

	spin_lock(&discovery_listener_lock);
	list_add_tail(&listener->list, &discovery_listener_list);
	spin_unlock(&discovery_listener_lock);
}

static void (soo_stream_recv)(sl_desc_t *sl_desc, void *data, size_t size) {

	lprintk("## got %d bytes\n", size);

}

/*
 * Testing RT task to send a stream to a specific smart object.
 * Helpful to perform assessment of the wireless transmission.
 */
static void soo_stream_task_fn(void *args) {

	sl_desc_t *sl_desc;
	neighbour_desc_t *dst;
	char *data = "Hello me";

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	sl_desc = sl_register(SL_REQ_PEER, SL_IF_WLAN, SL_MODE_UNICAST);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	sl_desc = sl_register(SL_REQ_PEER, SL_IF_ETH, SL_MODE_UNICAST);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	rtdm_sl_set_recv_callback(sl_desc, soo_stream_recv);

	while (true) {
		lprintk("### streaming now...\n");

		discovery_dump_neighbours();
		//neighbour_list_protection(true);

		/* Check if we are first? */
		//dst = list_first_entry(&neighbour_list, neighbour_desc_t, list);

		//neighbour_list_protection(false);

		//if (&dst->list != &neighbour_list) {


			//rtdm_sl_send(sl_desc, data, strlen(data)+1, &dst->agencyUID, 10);
			//sender_xmit(sl_desc, data, strlen(data)+1, true);

		//}


		rtdm_task_wait_period(NULL);
	}

}


/*
 * Main initialization function of the Discovery functional block
 */
void discovery_init(void) {
	INIT_LIST_HEAD(&neighbour_list);

	INIT_LIST_HEAD(&discovery_listener_list);
	spin_lock_init(&discovery_listener_lock);

	INIT_LIST_HEAD(&discovery_pending_add_list);
	INIT_LIST_HEAD(&neigh_blacklist);

	/* Register a new requester in Soolink for Discovery. */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_WLAN, SL_MODE_BROADCAST);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_ETH, SL_MODE_BROADCAST);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	DBG_BUFFER(&discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

	lprintk("Soolink Discovery init...\n");
}

void discovery_enable(void) {
	discovery_enabled = true;
}

void discovery_disable(void) {
	discovery_enabled = false;
}

/**
 * This function should be called from CPU #0.
 */
void discovery_start(void) {

	/* Already enabled? */
	if (discovery_enabled)
		return ;

	discovery_enable();

	rtdm_task_init(&rt_watch_loop_task, "Discovery", iamasoo_task_fn, NULL, DISCOVERY_TASK_PRIO, DISCOVERY_TASK_PERIOD);
	rtdm_task_init(&rt_soo_stream_task, "SOO-streaming", soo_stream_task_fn, NULL, DISCOVERY_TASK_PRIO, DISCOVERY_TASK_PERIOD);
}
