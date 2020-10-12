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
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <soo/soolink/discovery.h>
#include <soo/soolink/sender.h>

#include <soo/core/device_access.h>
#include <soo/core/sysfs.h>
#include <soo/core/asf.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

#include <soo/debug/bandwidth.h>

#undef CONFIG_ARM_PSCI

static neighbour_desc_t *ourself = NULL;

static bool discovery_enabled = false;

static struct list_head neighbour_list;
iamasoo_pkt_t iamasoo_beacon_pkt;

static struct list_head discovery_listener_list;
static bool __neighbour_list_protected;

/* Used to maintain new discovered neighbours if the main list is protected. */
static struct list_head discovery_pending_add_list;

static volatile uint32_t neighbor_count = 0;

typedef struct {
	neighbour_desc_t *neighbour;
	struct list_head list;
} pending_update_t;

/*
 * Used to maintain updated neighbours if the main list is protected.
 * This list is composed of pending_update_t items.
 */
static struct list_head discovery_pending_update_list;

static struct list_head neigh_blacklist;

static spinlock_t discovery_listener_lock;

static sl_desc_t *discovery_sl_desc;

/*
 * Update our agencyUID according to what device access says.
 */
void discovery_update_ourself(agencyUID_t *agencyUID) {
	memcpy(&ourself->agencyUID, agencyUID, SOO_AGENCY_UID_SIZE);
}

/**
 * Call the add callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_add_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_listener;

	if (neighbour->plugin)
		neighbor_count++;

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

	if (neighbour->plugin)
		neighbor_count--;

	list_for_each(cur, &discovery_listener_list) {
		cur_neighbour = list_entry(cur, discovery_listener_t, list);

		if (cur_neighbour->remove_neighbour_callback)
			cur_neighbour->remove_neighbour_callback(neighbour);
	}

	if (neighbour->plugin)
		/* Inform the plugin layer to remove the associated MAC address (if any) */
		detach_agencyUID(&neighbour->agencyUID);
}

/**
 * Call the update callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_update_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_neighbour;

	list_for_each(cur, &discovery_listener_list) {
		cur_neighbour = list_entry(cur, discovery_listener_t, list);

		if (cur_neighbour->update_neighbour_priv_callback)
			cur_neighbour->update_neighbour_priv_callback(neighbour);
	}
}
/**
 * Call the update callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static int callbacks_get_neighbour_priv(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_neighbour;

	list_for_each(cur, &discovery_listener_list) {
		cur_neighbour = list_entry(cur, discovery_listener_t, list);

		if (cur_neighbour->get_neighbour_priv_callback)
			/* Currently, only one listener can use this feature. */
			return cur_neighbour->get_neighbour_priv_callback(neighbour);
	}
	return 0;
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

			pr_cont("[soo:soolink:discovery] %s blacklist neighbour: ", __func__);
			printk_buffer(&cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

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
	int ret = 0;
	neighbour_desc_t *cur_neighbour;

	/* If the list is empty, add the neighbour to it */
	if (unlikely(list_empty(&neighbour_list)))

		list_add_tail(&neighbour->list, &neighbour_list);

	/* cur_neighbour is currently the last element of the list */
	else {

		/* Walk the list until we find the right place in ascending sort. */
		list_for_each_entry(cur_neighbour, &neighbour_list, list) {

			ret = memcmp(&neighbour->agencyUID, &cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			if (ret < 0) {
				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&neighbour->list, &cur_neighbour->list);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (&cur_neighbour->list == &neighbour_list)
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

	/* If the list is protected, it will be added later during the unprotect operation. */
	if (__neighbour_list_protected) {

		/* Check if already known in the add pending list */
		list_for_each_entry(cur_neighbour, &discovery_pending_add_list, list) {
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

/*
 * Update a new neighbour in the list.
 * The lock on the list must be taken.
 */
static void update_neighbour(neighbour_desc_t *neighbour) {
	bool found = false;
	pending_update_t *pending_update = NULL;

	/* If the list is protected, it will be added later during the unprotect operation. */
	if (__neighbour_list_protected) {

		/* Check if already known in the add pending list */
		list_for_each_entry(pending_update, &discovery_pending_update_list, list) {
			if (pending_update->neighbour == neighbour) {
				found = true;
				break;
			}
		}
		if (!found) {
			pending_update = kzalloc(sizeof(pending_update_t), GFP_ATOMIC);
			BUG_ON(!pending_update);

			pending_update->neighbour = neighbour;
			list_add(&pending_update->list, &discovery_pending_update_list);
		}
	} else
		callbacks_update_neighbour(neighbour);
}

/*
 * Build a byte area containing all our neighbours (our friends :-))
 */
void concat_friends(uint8_t *friends) {
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	uint8_t *pos = friends;

	spin_lock(&discovery_listener_lock);

	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		memcpy(pos, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		pos += SOO_AGENCY_UID_SIZE;
	}

	spin_unlock(&discovery_listener_lock);
}

/*
 * Retrieve the list of friends from a Iamasoo packet and put them in the appropriate
 * field of neighbour_dest_c.
 * It is assumed that discovery_listener_lock is hold.
 */
void expand_friends(uint8_t *friends, int friends_count, neighbour_desc_t *neighbour_desc) {
	int i;
	agencyUID_t *friend;

	if (!friends_count)
		return ;

	for (i = 0; i < friends_count; i++) {
		friend = kzalloc(sizeof(agencyUID_t), GFP_ATOMIC);
		BUG_ON(!friend);

		memcpy(&friend->id, friends, SOO_AGENCY_UID_SIZE);

		list_add_tail(&friend->list, &neighbour_desc->friends);

		friends += SOO_AGENCY_UID_SIZE;
	}
}

/*
 * Reset the list of friends (empty the list of friends)
 * It is assumed that discovery_listener_lock is hold.
 */
void reset_friends(struct list_head *friends) {
	struct list_head *cur, *tmp;
	agencyUID_t *friend;

	list_for_each_safe(cur, tmp, friends) {
		friend = list_entry(cur, agencyUID_t, list);

		list_del(cur);
		kfree(friend);
	}
}

/**
 * Asynchronous callback function called when a Iamasoo packet is received.
 */
void discovery_rx(plugin_desc_t *plugin_desc, void *data, size_t size, uint8_t *mac_src) {
	bool known_neighbour = false;
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	iamasoo_pkt_t *iamasoo_pkt;

	if (!discovery_enabled)
		return ;

	/* Beacon decryption */
#ifdef CONFIG_ARM_PSCI
	size = asf_decrypt(ASF_KEY_COM, (uint8_t *) data, size, (uint8_t **) &iamasoo_pkt);
#else
	iamasoo_pkt = (iamasoo_pkt_t *) data;
#endif

	/* Check if there is a binding with the MAC address already. */
	attach_agencyUID((agencyUID_t *) iamasoo_pkt->agencyUID, mac_src);

	spin_lock(&discovery_listener_lock);

	/* Look for the neighbour in the list */
	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);
		if (!memcmp(&neighbour->agencyUID, iamasoo_pkt->agencyUID, SOO_AGENCY_UID_SIZE)) {
			known_neighbour = true;
			break;
		}
	}

	/* Do not try to add a blacklisted neighbour */
	if (!known_neighbour) {
		list_for_each(cur, &neigh_blacklist) {
			neighbour = list_entry(cur, neighbour_desc_t, list);
			if (!memcmp(&neighbour->agencyUID, iamasoo_pkt->agencyUID, SOO_AGENCY_UID_SIZE)) {
				known_neighbour = true;
				break;
			}
		}
	}

	/* If the neighbour is not in the list... */
	if (!known_neighbour) {

		/* Add the neighbour in the list */
		neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
		if (!neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		memcpy(&neighbour->agencyUID, iamasoo_pkt->agencyUID, SOO_AGENCY_UID_SIZE);
		memcpy(neighbour->name, iamasoo_pkt->name, SOO_NAME_SIZE);

		neighbour->missing_tick = 0;
		neighbour->present = true;
		neighbour->plugin = plugin_desc;

		if (iamasoo_pkt->priv_len > 0) {
			/* Retrieve the private data */

			neighbour->priv = kzalloc(iamasoo_pkt->priv_len, GFP_ATOMIC);
			BUG_ON(!neighbour->priv);

			memcpy(neighbour->priv, iamasoo_pkt->extra, iamasoo_pkt->priv_len);
		}

		INIT_LIST_HEAD(&neighbour->friends);

		/* Expand the list of friends and put them in the neighbour_desc
		 * according to the iamasoo_pkt_t structure
		 */

		expand_friends(&iamasoo_pkt->extra[iamasoo_pkt->priv_len], (size - sizeof(iamasoo_pkt_t) - iamasoo_pkt->priv_len) / SOO_AGENCY_UID_SIZE, neighbour);

		DBG("Add the neighbour: %s - ", neighbour->name);
		DBG_BUFFER(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

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

		/* Update the private data */

		/* Clear the current value */
		kfree(neighbour->priv);
		neighbour->priv = NULL;

		if (iamasoo_pkt->priv_len > 0) {
			/* Retrieve the private data */
			neighbour->priv = kzalloc(iamasoo_pkt->priv_len, GFP_ATOMIC);
			BUG_ON(!neighbour->priv);

			memcpy(neighbour->priv, iamasoo_pkt->extra, iamasoo_pkt->priv_len);
		}

		/* Update the friends of this neighbour */
		reset_friends(&neighbour->friends);

		/* Expand the list of friends and put them in the neighbour_desc
		 * according to the iamasoo_pkt_t structure
		 */
		expand_friends(&iamasoo_pkt->extra[iamasoo_pkt->priv_len], (size - sizeof(iamasoo_pkt_t) - iamasoo_pkt->priv_len ) / SOO_AGENCY_UID_SIZE, neighbour);

		/* Call the update callbacks of listeners (currently one) */
		update_neighbour(neighbour);

	}

#ifdef CONFIG_ARM_PSCI
	kfree(iamasoo_pkt);
#endif

	spin_unlock(&discovery_listener_lock);
}

/**
 * Send a Iamasoo beacon.
 * All agencyUIDs of our neighbourhood will be concatened and referenced by the
 * field <friends>.
 */
static void send_beacon(void) {
	iamasoo_pkt_t *iamasoo_pkt;
	uint32_t size;
	uint8_t priv_len = 0;
#ifdef CONFIG_ARM_PSCI
	uint8_t *iamasoo_pkt_crypt;
#endif

	/* If the agency UID has not been initialized yet, do not send any Iamasoo beacon */
	if (unlikely(!agencyUID_is_valid(get_my_agencyUID())))
		return ;

	/* Prepare to broadcast */
	BUG_ON(memcmp(&discovery_sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE));

	/* Allocation of the Iamasoo beacon according to the structure and the number of our neighbours including us. */
	size = sizeof(iamasoo_pkt_t) + (discovery_neighbour_count() + 1) * SOO_AGENCY_UID_SIZE;

	if (!ourself) {
		list_for_each_entry(ourself, &neighbour_list, list) {
			if (!ourself->plugin)
				break;
		}
		if (&ourself->list == &neighbour_list)
			ourself = NULL;
	}

	if (ourself) {
		/* Collect private data from data link */
		priv_len = callbacks_get_neighbour_priv(ourself);
		size += priv_len;
	}

	iamasoo_pkt = kzalloc(size, GFP_ATOMIC);

	/* Copy the agency UID and the name of this Smart Object into the beacon packet */
	memcpy(&iamasoo_pkt->agencyUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
	memset(iamasoo_pkt->name, 0, SOO_NAME_SIZE);
	devaccess_get_soo_name(iamasoo_pkt->name);

	/* Add the private data if any */
	if (priv_len) {
		iamasoo_pkt->priv_len = priv_len;
		memcpy(iamasoo_pkt->extra, ourself->priv, priv_len);
	}

	concat_friends(&iamasoo_pkt->extra[priv_len]);

	/* Beacon encryption */
#ifdef CONFIG_ARM_PSCI
	size = asf_encrypt(ASF_KEY_COM, (uint8_t *) iamasoo_pkt, size, &iamasoo_pkt_crypt);
#endif
	/* Now send the Iamasoo beacon to the sender */
	DBG("%s: sending to: ", __func__);
	DBG_BUFFER(&discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

#ifdef CONFIG_ARM_PSCI
	sender_tx(discovery_sl_desc, iamasoo_pkt_crypt, size, true);
#else
	sender_tx(discovery_sl_desc, iamasoo_pkt, size, true);
#endif

#ifdef CONFIG_ARM_PSCI
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
	uint8_t soo_name[SOO_NAME_SIZE];

	/* At first, we insert ourself in the neighbour list;
	 * this is useful to keep a sorted list with all participants in a
	 * round of broadcast and to identify the next speaker.
	 */

	spin_lock(&discovery_listener_lock);

	/* Add the neighbour in the list */
	neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
	BUG_ON(!neighbour);

	memcpy(&neighbour->agencyUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
	devaccess_get_soo_name(soo_name);
	memcpy(&neighbour->name, soo_name, SOO_NAME_SIZE);

	neighbour->missing_tick = 0;
	neighbour->present = true;

	pr_cont("[soo:soolink] Adding ourself (%s) - ", neighbour->name);
	printk_buffer(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

	__add_neighbour(neighbour);

	spin_unlock(&discovery_listener_lock);

	discovery_enable();

	while (1) {

		msleep(DISCOVERY_TASK_PERIOD_MS);

		if (!discovery_enabled)
			continue;

		/* Broadcast the beacon over all plugins */
		send_beacon();

		spin_lock(&discovery_listener_lock);

		/* Increment the missing_tick, over the neighbours */
		list_for_each(cur, &neighbour_list) {
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
		list_for_each_safe(cur, tmp, &neighbour_list) {
			neighbour = list_entry(cur, neighbour_desc_t, list);

			/* If a neighbour is dead, delete it from the list */
			if (neighbour->missing_tick >= SOOLINK_MISSING_TICK_MAX) {

				if (__neighbour_list_protected)
					neighbour->present = false;  /* It will be removed later, during unprotect operation */
				else {
					/* Call the neighbour remove callbacks */
					callbacks_remove_neighbour(neighbour);

					/* Remove all friends */
					reset_friends(&neighbour->friends);

					/* Release the private data memory if any */
					if (neighbour->priv)
						kfree(neighbour->priv);

					printk("[soo:soolink] Delete the neighbour: ");
					printk_buffer(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					list_del(cur);
					kfree(neighbour);
				}
			}
		}

		spin_unlock(&discovery_listener_lock);
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

	spin_lock(&discovery_listener_lock);

	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		/* Add the neighbour in the list */
		new_neighbour = (neighbour_desc_t *) kzalloc(sizeof(neighbour_desc_t), GFP_ATOMIC);
		if (!new_neighbour) {
			lprintk("Cannot allocate a new neighbour\n");
			BUG();
		}

		/* Copy the attributes */
		memcpy(&new_neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		memcpy(new_neighbour->name, neighbour->name, SOO_NAME_SIZE);
		new_neighbour->plugin = neighbour->plugin;
		new_neighbour->missing_tick = neighbour->missing_tick;

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
	neighbour_desc_t *neighbour = NULL, *tmp;
	pending_update_t *pending_update = NULL, *tmp2;

	spin_lock(&discovery_listener_lock);

	BUG_ON(protect && __neighbour_list_protected);
	BUG_ON(!protect && !__neighbour_list_protected);

	/* Check for a need of synchronization with the pending list. */
	if (!protect) {

		/* Possible pending addings ? */
		list_for_each_entry_safe(neighbour, tmp, &discovery_pending_add_list, list) {

			/* Remove the entry */
			list_del(&neighbour->list);

			__add_neighbour(neighbour);
		}

		/* Possible pending updates ? */
		list_for_each_entry_safe(pending_update, tmp2, &discovery_pending_update_list, list) {

			/* Remove the entry */
			list_del(&pending_update->list);

			callbacks_update_neighbour(pending_update->neighbour);
			kfree(pending_update);
		}

		/* We check if some neighbour disappeared in the meanwhile */
		list_for_each_entry_safe(neighbour, tmp, &neighbour_list, list) {

			if (!neighbour->present) {

				/* Call the neighbour remove callbacks */
				callbacks_remove_neighbour(neighbour);

				list_del(&neighbour->list);
				kfree(neighbour);
			}
		}
	}

	__neighbour_list_protected = protect;

	spin_unlock(&discovery_listener_lock);
}

/**
 * Dump the active neighbour list.
 */
void discovery_dump_neighbours(void) {
	struct list_head *cur, *cur_friend;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;
	agencyUID_t *friend;

	spin_lock(&discovery_listener_lock);

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&neighbour_list)) {
		printk("[soo:soolink:discovery] No neighbour\n");
		spin_unlock(&discovery_listener_lock);
		return;
	}

	list_for_each(cur, &neighbour_list) {

		neighbour = list_entry(cur, neighbour_desc_t, list);

		lprintk("[soo:soolink:discovery] Neighbour %d: %s - ", count+1, neighbour->name);
		lprintk_buffer(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		lprintk("\n");

		if (!neighbour->plugin)
			lprintk("[soo:soolink:discovery] ** ourself **\n");
		else {
			lprintk("[soo:soolink:discovery]      ** Friends: **\n");
			list_for_each(cur_friend, &neighbour->friends) {
				friend = list_entry(cur_friend, agencyUID_t, list);
				pr_cont("[soo:soolink:discovery] ");
				lprintk_buffer(&friend->id, SOO_AGENCY_UID_SIZE);
				lprintk("\n");
			}
		}

		count++;
	}

	spin_unlock(&discovery_listener_lock);
}

/**
 * Return the number of neighbours currently known.
 */
uint32_t discovery_neighbour_count(void) {
	return neighbor_count;
}

/**
 * Register a Discovery listener.
 */
void discovery_listener_register(discovery_listener_t *listener) {

	spin_lock(&discovery_listener_lock);
	list_add_tail(&listener->list, &discovery_listener_list);
	spin_unlock(&discovery_listener_lock);
}

void neighbours_read(char *str) {
	sprintf(str, "%d", discovery_neighbour_count());
}

#if 0 /* Debugging purposes */

static int count = 0;
sl_desc_t *sl_desc;

#define BUFFER_SIZE 16*1024*1024

static unsigned char buffer[BUFFER_SIZE];

static int soo_stream_task_rx_fn(void *args) {
	uint32_t size;
	void *data;
	int i;

	while (true){
		size = sl_recv(sl_desc, &data);

		for (i = 0; i < BUFFER_SIZE; i++)
			if (((unsigned char *) data)[i] != buffer[i]) {
				printk("## Data corruption : failure on byte %d\n", i);
				break;
			}

		if (i == BUFFER_SIZE) {
			count++;
			lprintk("## ******************** Got a buffer (count %d got %d bytes)\n", count, size);
		}

		/* Must release the allocated buffer */
		vfree(data);
	}

	return 0;
}

void stream_count_read(char *str) {
	sprintf(str, "%d", count);
}

/*
 * Testing RT task to send a stream to a specific smart object.
 * This is mainly used for debugging purposes and performance assessment.
 */
static int soo_stream_task_tx_fn(void *args) {
	int i;

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	sl_desc = sl_register(SL_REQ_DCM, SL_IF_WLAN, SL_MODE_UNIBROAD);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	sl_desc = sl_register(SL_REQ_DCM, SL_IF_ETH, SL_MODE_UNIBROAD);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	for (i = 0; i < BUFFER_SIZE; i++)
		buffer[i] = i;

	soo_sysfs_register(stream_count, stream_count_read, NULL);
	soo_sysfs_register(neighbours, neighbours_read, NULL);
#if 1
	while (true) {

		if (discovery_neighbour_count() > 0) {
			lprintk("*** sending buffer ****\n");
			sl_send(sl_desc, buffer, BUFFER_SIZE, get_null_agencyUID(), 10);

			lprintk("*** sending COMPLETE ***\n");
			sl_send(sl_desc, NULL, 0, get_null_agencyUID(), 10);

			lprintk("*** End. ***\n");
		} else
			schedule();

		/* rtdm_task_wait_period(NULL); */
	}
#endif
	return 0;
}

#endif /* 0 */


/*
 * Main initialization function of the Discovery functional block
 */
void discovery_init(void) {

	lprintk("Soolink Discovery init...\n");

	INIT_LIST_HEAD(&neighbour_list);

	INIT_LIST_HEAD(&discovery_listener_list);
	spin_lock_init(&discovery_listener_lock);

	INIT_LIST_HEAD(&discovery_pending_add_list);
	INIT_LIST_HEAD(&discovery_pending_update_list);

	INIT_LIST_HEAD(&neigh_blacklist);

	/* Create an entry in sysfs to export the number of neighbours to the user space */
	soo_sysfs_register(neighbours, neighbours_read, NULL);

	/* Register a new requester in Soolink for Discovery. */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_WLAN, SL_MODE_BROADCAST);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	discovery_sl_desc = sl_register(SL_REQ_DISCOVERY, SL_IF_ETH, SL_MODE_BROADCAST);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	DBG_BUFFER(&discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);
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

	kthread_run(iamasoo_task_fn, NULL, "iamasoo_task");

	/* Activated if necessary for debugging purposes and performance assessment. */
#if 0
	kthread_run(soo_stream_task_tx_fn, NULL, "rt_soo_stream_task_tx");
	kthread_run(soo_stream_task_rx_fn, NULL, "rt_soo_stream_task_rx");
#endif

}
