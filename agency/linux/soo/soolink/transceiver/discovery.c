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

#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <soo/sooenv.h>

#include <soo/soolink/discovery.h>
#include <soo/soolink/transceiver.h>

#include <soo/core/device_access.h>
#include <soo/core/sysfs.h>
#include <soo/core/asf.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

#include <soo/debug/bandwidth.h>

typedef struct {
	neighbour_desc_t *neighbour;
	struct list_head list;
} pending_update_t;

struct soo_discovery_env {

	neighbour_desc_t *ourself;
	bool discovery_enabled;

	struct list_head neighbour_list;

	struct list_head discovery_listener_list;

	bool __neighbour_list_protected;

	/* Used to maintain new discovered neighbours if the main list is protected. */
	struct list_head discovery_pending_add_list;

	volatile uint32_t neighbor_count;

	struct list_head discovery_pending_update_list;

	struct mutex discovery_listener_lock;

	char blacklisted_soo[BLACKLIST_MAX_SZ][SOO_NAME_SIZE];
	uint32_t blacklist_idx;

	sl_desc_t *discovery_sl_desc;
};

/*
 * Update our agencyUID according to what device access says.
 */
void discovery_update_ourself(uint64_t agencyUID) {
	current_soo_discovery->ourself->agencyUID = agencyUID;
}

/**
 * Call the add callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_add_neighbour(neighbour_desc_t *neighbour) {
	discovery_listener_t *cur_listener;

	if (neighbour->plugin)
		current_soo_discovery->neighbor_count++;

	list_for_each_entry(cur_listener, &current_soo_discovery->discovery_listener_list, list) {
		if (cur_listener->add_neighbour_callback)
			cur_listener->add_neighbour_callback(neighbour);
	}
}

/**
 * Call the remove callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_remove_neighbour(neighbour_desc_t *neighbour) {
	discovery_listener_t *cur_neighbour;

	if (neighbour->plugin)
		current_soo_discovery->neighbor_count--;

	list_for_each_entry(cur_neighbour, &current_soo_discovery->discovery_listener_list, list) {

		if (cur_neighbour->remove_neighbour_callback)
			cur_neighbour->remove_neighbour_callback(neighbour);
	}

	if (neighbour->plugin)
		/* Inform the plugin layer to remove the associated MAC address (if any) */
		detach_agencyUID(neighbour->agencyUID);
}

/**
 * Call the update callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_update_neighbour(neighbour_desc_t *neighbour) {
	discovery_listener_t *cur_neighbour;

	list_for_each_entry(cur_neighbour, &current_soo_discovery->discovery_listener_list, list) {

		if (cur_neighbour->update_neighbour_callback)
			cur_neighbour->update_neighbour_callback(neighbour);
	}
}

/*
 * Add the neighbour in the list definitively.
 * The lock on the list must be taken.
 */
static void __add_neighbour(neighbour_desc_t *neighbour) {
	neighbour_desc_t *cur_neighbour;

	/* If the list is empty, add the neighbour to it */
	if (unlikely(list_empty(&current_soo_discovery->neighbour_list)))

		list_add_tail(&neighbour->list, &current_soo_discovery->neighbour_list);

	/* cur_neighbour is currently the last element of the list */
	else {

		/* Walk the list until we find the right place in ascending sort. */
		list_for_each_entry(cur_neighbour, &current_soo_discovery->neighbour_list, list) {

			if (neighbour->agencyUID > cur_neighbour->agencyUID) {
				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&neighbour->list, &cur_neighbour->list);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (&cur_neighbour->list == &current_soo_discovery->neighbour_list)
			list_add_tail(&neighbour->list, &current_soo_discovery->neighbour_list);
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

	/* If the list is protected, it will be added later during the unprotect operation. */
	if (current_soo_discovery->__neighbour_list_protected) {

		/* Check if already known in the add pending list */
		list_for_each_entry(cur_neighbour, &current_soo_discovery->discovery_pending_add_list, list) {
			if (cur_neighbour->agencyUID == neighbour->agencyUID) {
				found = true;
				break;
			}
		}
		if (!found)
			list_add(&neighbour->list, &current_soo_discovery->discovery_pending_add_list);
	} else
		__add_neighbour(neighbour);
}

/*
 * Update a new neighbour in the list.
 * The lock on the list must be taken.
 */
static void update_neighbour(neighbour_desc_t *neighbour) {
	bool found = false;
	pending_update_t *pending_update = NULL;

	/* If the list is protected, it will be added later during the unprotect operation. */
	if (current_soo_discovery->__neighbour_list_protected) {

		/* Check if already known in the add pending list */
		list_for_each_entry(pending_update, &current_soo_discovery->discovery_pending_update_list, list) {
			if (pending_update->neighbour == neighbour) {
				found = true;
				break;
			}
		}
		if (!found) {
			pending_update = kzalloc(sizeof(pending_update_t), GFP_ATOMIC);
			BUG_ON(!pending_update);

			pending_update->neighbour = neighbour;
			list_add(&pending_update->list, &current_soo_discovery->discovery_pending_update_list);
		}
	} else
		callbacks_update_neighbour(neighbour);
}

/**
 * Asynchronous callback function called when a Iamasoo packet is received.
 */
void discovery_rx(plugin_desc_t *plugin_desc, void *data, size_t size, uint8_t *mac_src) {
	bool known_neighbour = false;
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	iamasoo_pkt_t *iamasoo_pkt;
	int i;

	if (!current_soo_discovery->discovery_enabled)
		return ;

	/* Beacon decryption */
#ifdef CONFIG_SOO_CORE_ASF
	size = asf_decrypt(ASF_KEY_COM, (uint8_t *) data, size, (uint8_t **) &iamasoo_pkt);
#else
	iamasoo_pkt = (iamasoo_pkt_t *) data;
#endif


	soo_log("[soo:soolink:discovery] Got a beacon\n");

	for (i = 0; i < current_soo_discovery->blacklist_idx; ++i) {
		if (!strcmp(iamasoo_pkt->name, current_soo_discovery->blacklisted_soo[i])) {
			mutex_unlock(&current_soo_discovery->discovery_listener_lock);
#ifdef CONFIG_SOO_CORE_ASF
			kfree(iamasoo_pkt);
#endif				
			return;
		}
	}


	/* Check if there is a binding with the MAC address already. */
	attach_agencyUID(iamasoo_pkt->agencyUID, mac_src);

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	/* Look for the neighbour in the list */
	list_for_each(cur, &current_soo_discovery->neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);
		if (neighbour->agencyUID == iamasoo_pkt->agencyUID) {
			known_neighbour = true;
			break;
		}
	}

	/* If the neighbour is not in the list... */
	if (!known_neighbour) {

		/* Add the neighbour in the list */
		neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_KERNEL);
		if (!neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		neighbour->agencyUID = iamasoo_pkt->agencyUID;
		memcpy(neighbour->name, iamasoo_pkt->name, SOO_NAME_SIZE);

		neighbour->missing_tick = 0;
		neighbour->present = true;
		neighbour->plugin = plugin_desc;

		soo_log("[soo:soolink:discovery] Adding the neighbour: %s - ", neighbour->name);
		soo_log_printlnUID(neighbour->agencyUID);

		add_neighbour(neighbour);

	} else {

		/* If the neighbour is already known */

		DBG("Reset the neighbour's missing_tick to 0: ");
		DBG_BUFFER(&cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		/* Update the name of the Smart Object (it can change) */
		memcpy(neighbour->name, iamasoo_pkt->name, SOO_NAME_SIZE);

		/* Reset the neighbour's missing_tick to 0. */
		neighbour->missing_tick = 0;
		neighbour->present = true;  /* If disappeared in between */

		/* Call the update callbacks of listeners (currently one) */
		update_neighbour(neighbour);

	}

#ifdef CONFIG_SOO_CORE_ASF
	kfree(iamasoo_pkt);
#endif

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);

#if 0
	discovery_dump_neighbours();
#endif
}

/**
 * Send a Iamasoo beacon.
 */
static void send_beacon(void) {
	iamasoo_pkt_t *iamasoo_pkt;

#ifdef CONFIG_SOO_CORE_ASF
	uint8_t *iamasoo_pkt_crypt;
#endif

	/* If the agency UID has not been initialized yet, do not send any Iamasoo beacon */
	if (!current_soo->agencyUID)
		return ;

	/* Prepare to broadcast */
	BUG_ON(current_soo_discovery->discovery_sl_desc->agencyUID_to != 0);

	if (!current_soo_discovery->ourself) {
		list_for_each_entry(current_soo_discovery->ourself, &current_soo_discovery->neighbour_list, list) {
			if (!current_soo_discovery->ourself->plugin)
				break;
		}
		if (&current_soo_discovery->ourself->list == &current_soo_discovery->neighbour_list)
			current_soo_discovery->ourself = NULL;
	}

	iamasoo_pkt = kzalloc(sizeof(iamasoo_pkt_t), GFP_ATOMIC);
	BUG_ON(!iamasoo_pkt);

	/* Copy the agency UID and the name of this Smart Object into the beacon packet */
	iamasoo_pkt->agencyUID = current_soo->agencyUID;

	memset(iamasoo_pkt->name, 0, SOO_NAME_SIZE);
	strcpy(iamasoo_pkt->name, current_soo->name);

	/* Beacon encryption */
#ifdef CONFIG_SOO_CORE_ASF
	size = asf_encrypt(ASF_KEY_COM, (uint8_t *) iamasoo_pkt, size, &iamasoo_pkt_crypt);
#endif
	/* Now send the Iamasoo beacon to the sender */
	soo_log("[soo:soolink:discovery] %s: sending to: ", __func__);
	soo_log_printlnUID(current_soo_discovery->discovery_sl_desc->agencyUID_to);

#ifdef CONFIG_SOO_CORE_ASF
	sender_tx(current_soo_discovery->discovery_sl_desc, iamasoo_pkt_crypt, size, true);
#else
	sender_tx(current_soo_discovery->discovery_sl_desc, iamasoo_pkt, sizeof(iamasoo_pkt_t), true);
#endif

#ifdef CONFIG_SOO_CORE_ASF
	kfree(iamasoo_pkt_crypt);
#endif
	kfree(iamasoo_pkt);
}

/**
 * Infinite loop that checks if the neighbours are alive.
 */
static int iamasoo_task_fn(void *args) {
	struct list_head *cur, *tmp;
	neighbour_desc_t *neighbour;

	/* At first, we insert ourself in the neighbour list;
	 * this is useful to keep a sorted list with all participants in a
	 * round of broadcast and to identify the next speaker.
	 */

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	/* Add the neighbour in the list */
	neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
	BUG_ON(!neighbour);

	neighbour->agencyUID = current_soo->agencyUID;

	strcpy(neighbour->name, current_soo->name);

	neighbour->missing_tick = 0;
	neighbour->present = true;

	soo_log("[soo:soolink:discovery] Adding ourself (%s) - ", neighbour->name);
	soo_log_printlnUID(neighbour->agencyUID);

	__add_neighbour(neighbour);

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);

	discovery_enable();

	while (1) {

		msleep(DISCOVERY_TASK_PERIOD_MS);

		if (!current_soo_discovery->discovery_enabled)
			continue;

		/* Broadcast the beacon over all plugins */
		send_beacon();

		mutex_lock(&current_soo_discovery->discovery_listener_lock);

		/* Increment the missing_tick, over the neighbours */
		list_for_each(cur, &current_soo_discovery->neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			/* Only for *real* neighbours */
			if (neighbour->plugin)
				neighbour->missing_tick++;
		}

		/* If the list is protected, we admit the possibility to have an entry which may disappear during
		 * the protected phase. However, it will stay in the list and will disappear once it will integrate
		 * the main list.
		 */

		/* Check if a neighbour is dead */
		list_for_each_safe(cur, tmp, &current_soo_discovery->neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			/* If a neighbour is dead, delete it from the list */
			if (neighbour->missing_tick >= SOOLINK_MISSING_TICK_MAX) {

				if (current_soo_discovery->__neighbour_list_protected)
					neighbour->present = false;  /* It will be removed later, during unprotect operation */
				else {
					/* Call the neighbour remove callbacks */
					callbacks_remove_neighbour(neighbour);

					soo_log("[soo:soolink:discovery] Delete the neighbour: ");
					soo_log_printlnUID(neighbour->agencyUID);

					list_del(cur);
					kfree(neighbour);
				}
			}
		}

		mutex_unlock(&current_soo_discovery->discovery_listener_lock);
	}

	return 0;
}

/**
 * Make a copy of the active neighbour list. The elements are allocated on-the-fly.
 * If there is no memory available and/or if the allocation has failed, the function will enter into bug.
 */
int discovery_get_neighbours(struct list_head *new_list) {
	struct list_head *cur;
	neighbour_desc_t *neighbour, *new_neighbour;
	uint32_t count = 0;

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	list_for_each(cur, &current_soo_discovery->neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		/* Add the neighbour in the list */
		new_neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
		if (!new_neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		/* Copy the attributes */
		new_neighbour->agencyUID = neighbour->agencyUID;
		memcpy(new_neighbour->name, neighbour->name, SOO_NAME_SIZE);
		new_neighbour->plugin = neighbour->plugin;
		new_neighbour->missing_tick = neighbour->missing_tick;

		/* Add the new element to the output list */
		list_add_tail(&new_neighbour->list, new_list);

		count++;
	}

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);

	return count;
}

/*
 * Enable the protection on the main neighbour list.
 * Used to prevent any changes during some operations.
 */
bool neighbour_list_protection(bool protect) {
	neighbour_desc_t *neighbour = NULL, *tmp;
	pending_update_t *pending_update = NULL, *tmp2;
	bool __old_state;

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	if (protect && current_soo_discovery->__neighbour_list_protected) {
		mutex_unlock(&current_soo_discovery->discovery_listener_lock);
		return current_soo_discovery->__neighbour_list_protected;
	}

	if (!protect && !current_soo_discovery->__neighbour_list_protected) {
		mutex_unlock(&current_soo_discovery->discovery_listener_lock);
		return current_soo_discovery->__neighbour_list_protected;
	}

	__old_state = current_soo_discovery->__neighbour_list_protected;

	/* Check for a need of synchronization with the pending list. */
	if (!protect) {

		/* Possible pending addings ? */
		list_for_each_entry_safe(neighbour, tmp, &current_soo_discovery->discovery_pending_add_list, list) {

			/* Remove the entry */
			list_del(&neighbour->list);

			__add_neighbour(neighbour);
		}

		/* Possible pending updates ? */
		list_for_each_entry_safe(pending_update, tmp2, &current_soo_discovery->discovery_pending_update_list, list) {

			/* Remove the entry */
			list_del(&pending_update->list);

			callbacks_update_neighbour(pending_update->neighbour);
			kfree(pending_update);
		}

		/* We check if some neighbour disappeared in the meanwhile */
		list_for_each_entry_safe(neighbour, tmp, &current_soo_discovery->neighbour_list, list) {

			if (!neighbour->present) {

				/* Call the neighbour remove callbacks */
				callbacks_remove_neighbour(neighbour);

				list_del(&neighbour->list);
				kfree(neighbour);
			}
		}
	}

	current_soo_discovery->__neighbour_list_protected = protect;

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);

	return __old_state;
}

/**
 * Dump the active neighbour list.
 */
void discovery_dump_neighbours(void) {
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&current_soo_discovery->neighbour_list)) {
		soo_log("[soo:soolink:discovery] No neighbour\n");
		mutex_unlock(&current_soo_discovery->discovery_listener_lock);
		return;
	}

	list_for_each(cur, &current_soo_discovery->neighbour_list) {

		neighbour = list_entry(cur, neighbour_desc_t, list);

		soo_log("[soo:soolink:discovery] Neighbour %d: %s - ", count+1, neighbour->name);
		soo_log_printlnUID(neighbour->agencyUID);

		if (!neighbour->plugin)
			soo_log("[soo:soolink:discovery] ** ourself **\n");

		count++;
	}

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);
}

/**
 * Return the number of neighbours currently known.
 */
uint32_t discovery_neighbour_count(void) {
	return current_soo_discovery->neighbor_count;
}


uint32_t discovery_blacklist_neighbour(char *neighbour_name) {
	struct list_head *cur, *tmp;
	neighbour_desc_t *neighbour;

	if (current_soo_discovery->blacklist_idx == BLACKLIST_MAX_SZ) {
		printk("[discovery]: Blacklist full, cannot add the SOO to it.\n");
		return -1;
	}
	DBG("Now blacklisting %s\n", neighbour_name);

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	strcpy(current_soo_discovery->blacklisted_soo[current_soo_discovery->blacklist_idx], neighbour_name);
	current_soo_discovery->blacklist_idx++;

	list_for_each_safe(cur, tmp, &current_soo_discovery->neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		if (!strcmp(neighbour_name, neighbour->name)) {
			if (current_soo_discovery->__neighbour_list_protected) {
				/* It will be removed later, during unprotect operation */
				neighbour->present = false;  
			} else {
				/* Call the neighbour remove callbacks */
				callbacks_remove_neighbour(neighbour);

				list_del(cur);
				kfree(neighbour);
				
			}
			mutex_unlock(&current_soo_discovery->discovery_listener_lock);
			return 0;
		}
	}

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);
	return 0;
}

/**
 * Register a Discovery listener.
 */
void discovery_listener_register(discovery_listener_t *listener) {

	mutex_lock(&current_soo_discovery->discovery_listener_lock);
	list_add_tail(&listener->list, &current_soo_discovery->discovery_listener_list);
	mutex_unlock(&current_soo_discovery->discovery_listener_lock);
}

void discovery_enable(void) {
	current_soo_discovery->discovery_enabled = true;
}

void discovery_disable(void) {
	current_soo_discovery->discovery_enabled = false;
}

void neighbours_read(char *str) {
	sprintf(str, "%d", discovery_neighbour_count());
}

void neighbours_ext_read(char *str) {
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;
	uint32_t char_cnt = 0;

	mutex_lock(&current_soo_discovery->discovery_listener_lock);

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&current_soo_discovery->neighbour_list)) {
		mutex_unlock(&current_soo_discovery->discovery_listener_lock);
		sprintf(str, "");
		return;
	}

	list_for_each(cur, &current_soo_discovery->neighbour_list) {

		neighbour = list_entry(cur, neighbour_desc_t, list);


		if (neighbour->plugin) {
			char_cnt += sprintf(str+char_cnt, "Neighbour %d (%llu) - %s: missed %u ticks\n", 
				count+1, 
				neighbour->agencyUID,
				neighbour->name,  
				neighbour->missing_tick);
		}


		count++;
	}

	mutex_unlock(&current_soo_discovery->discovery_listener_lock);
}

/*
 * Main initialization function of the Discovery functional block
 */
void discovery_init(void) {
	struct task_struct *__ts;
	int i;

	lprintk("SOOlink: Discovery init...\n");

	current_soo->soo_discovery = kzalloc(sizeof(struct soo_discovery_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_discovery);

	INIT_LIST_HEAD(&current_soo_discovery->neighbour_list);

	INIT_LIST_HEAD(&current_soo_discovery->discovery_listener_list);
	mutex_init(&current_soo_discovery->discovery_listener_lock);

	INIT_LIST_HEAD(&current_soo_discovery->discovery_pending_add_list);
	INIT_LIST_HEAD(&current_soo_discovery->discovery_pending_update_list);

	/* Register a new requester in Soolink for Discovery. */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	current_soo_discovery->discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_WLAN, SL_MODE_BROADCAST);
#elif defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	current_soo_discovery->discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_ETH, SL_MODE_BROADCAST);
#elif defined(CONFIG_SOOLINK_PLUGIN_SIMULATION)
	current_soo_discovery->discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_SIM, SL_MODE_BROADCAST);
#else
#error !! You must specify a plugin interface in the kernel configuration !!
#endif

	DBG_BUFFER(&current_soo_discovery->discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

	/* Enable the discovery function of SOOlink transceiver. */

	/* The first SOO id is 1 (SOO-1) */
	if (current_soo->id == 1) {
		lprintk("SOOlink: registering <neighbours> entry in /sys/soo/soolink...\n");

		/* Create an entry in sysfs to export the number of neighbours to the user space */
		soo_sysfs_register(neighbours, neighbours_read, NULL);

		lprintk("SOOlink: registering <neighbours_ext> entry in /sys/soo/soolink...\n");
		soo_sysfs_register(neighbours_ext, neighbours_ext_read, NULL);
	}

	__ts = kthread_create(iamasoo_task_fn, NULL, "iamasoo_task");
	BUG_ON(!__ts);

	for (i = 0; i < BLACKLIST_MAX_SZ; ++i) {
		memset(current_soo_discovery->blacklisted_soo[i], 0, SOO_NAME_SIZE);
	}

	add_thread(current_soo, __ts->pid);

	wake_up_process(__ts);
}


