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
#include <soo/core/sysfs.h>
#include <soo/core/asf.h>

#include <xenomai/rtdm/driver.h>

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

/**
 * Call the update callbacks in all the registered Discovery listeners.
 * The lock on the list must be taken.
 */
static void callbacks_update_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur;
	discovery_listener_t *cur_neighbour;

	if (__neighbour_list_protected)
		return ; /* Will be done later on. */

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
	int ret = 0;
	neighbour_desc_t *cur_neighbour;

	/* If the list is empty, add the neighbour to it */
	if (unlikely(list_empty(&neighbour_list)))

		list_add_tail(&neighbour->list, &neighbour_list);

	/* cur_neighbour is currently the last element of the list */
	else {

		/* Walk the list until we find the right place in ascending sort. */
		list_for_each(cur, &neighbour_list) {

			cur_neighbour = list_entry(cur, neighbour_desc_t, list);
			ret = memcmp(&neighbour->agencyUID, &cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			if (ret < 0) {
				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&neighbour->list, cur);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (cur == &neighbour_list)
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
void discovery_rx(plugin_desc_t *plugin_desc, void *data, size_t size) {
	bool known_neighbour = false;
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	iamasoo_pkt_t *iamasoo_pkt = (iamasoo_pkt_t *) data;

	if (!discovery_enabled)
		return ;

#if 0
	/* Beacon decryption */
	size = asf_decrypt(ASF_KEY_COM, (uint8_t *)data, size, (uint8_t **)&iamasoo_pkt);
#endif
lprintk("<");
	spin_lock(&discovery_listener_lock);
lprintk(">\n");

	/* Look for the neighbour in the list */
	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);
		if (!memcmp(&neighbour->agencyUID, data, SOO_AGENCY_UID_SIZE)) {
			known_neighbour = true;
			break;
		}
	}

	/* Do not try to add a blacklisted neighbour */
	if (!known_neighbour) {
		list_for_each(cur, &neigh_blacklist) {
			neighbour = list_entry(cur, neighbour_desc_t, list);
			if (!memcmp(&neighbour->agencyUID, data, SOO_AGENCY_UID_SIZE)) {
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
		callbacks_update_neighbour(neighbour);

	}

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
	static neighbour_desc_t *ourself = NULL;
	struct list_head *cur;
	uint8_t priv_len = 0;
	//uint8_t *iamasoo_pkt_crypt;

	/* If the agency UID has not been initialized yet, do not send any Iamasoo beacon */
	if (unlikely(!agencyUID_is_valid(get_my_agencyUID())))
		return ;

	/* Allocation of the Iamasoo beacon according to the structure and the number of our neighbours including us. */
	size = sizeof(iamasoo_pkt_t) + (discovery_neighbour_count() + 1) * SOO_AGENCY_UID_SIZE;

	if (!ourself) {
		list_for_each(cur, &neighbour_list) {
			ourself = list_entry(cur, neighbour_desc_t, list);
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

#if 0
	/* Beacon encryption */
	size = asf_encrypt(ASF_KEY_COM, (uint8_t *)&iamasoo_beacon_pkt, sizeof(iamasoo_pkt_t), &iamasoo_pkt_crypt);
#endif

	/* Now send the Iamasoo beacon to the sender */
	DBG("%s: sending to: ", __func__);
	DBG_BUFFER(&discovery_sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

	sender_xmit(discovery_sl_desc, iamasoo_pkt, size, true);

	kfree(iamasoo_pkt);
}

/**
 * Infinite loop that checks if the neighbours are alive.
 */
static void iamasoo_task_fn(void *args) {
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

	DBG("Adding ourself (%s) - ", neighbour->name);
	DBG_BUFFER(&neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

	add_neighbour(neighbour);

	spin_unlock(&discovery_listener_lock);

	discovery_enable();

	while (1) {
		rtdm_task_wait_period(NULL);

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
	struct list_head *cur, *cur_friend;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;
	agencyUID_t *friend;

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

		if (!neighbour->plugin) {
			lprintk(" ** ourself **");
			lprintk("\n");
		} else {
			lprintk("\n     ** Friends: **\n");
			list_for_each(cur_friend, &neighbour->friends) {
				friend = list_entry(cur_friend, agencyUID_t, list);
				lprintk_buffer(&friend->id, SOO_AGENCY_UID_SIZE);
				lprintk("\n");
			}
			lprintk("\n");
		}

		count++;
	}

	spin_unlock(&discovery_listener_lock);
}

/**
 * Return the number of neighbours currently known.
 */
uint32_t discovery_neighbour_count(void) {
	struct list_head *cur;
	neighbour_desc_t *neighbour;
	uint32_t count = 0;

	spin_lock(&discovery_listener_lock);

	list_for_each(cur, &neighbour_list) {
		neighbour = list_entry(cur, neighbour_desc_t, list);

		/* Check the entry is not ourself. */
		if (neighbour->plugin)
			count++;
	}

	spin_unlock(&discovery_listener_lock);

	return count;
}

/**
 * Register a Discovery listener.
 */
void discovery_listener_register(discovery_listener_t *listener) {

	spin_lock(&discovery_listener_lock);
	list_add_tail(&listener->list, &discovery_listener_list);
	spin_unlock(&discovery_listener_lock);
}

static int count = 0;
static void (soo_stream_recv)(sl_desc_t *sl_desc, void *data, size_t size) {

	count++;
	lprintk("## ******************** Got a buffer (count %d got %d bytes)\n", count, size);

	/* Must release the allocated buffer */
	vfree(data);
}

#define BUFFER_SIZE 20*1024

static unsigned char buffer[BUFFER_SIZE];

void stream_count_read(char *str) {
	sprintf(str, "%d", count);
}

void neighbours_read(char *str) {
	sprintf(str, "%d", discovery_neighbour_count());
}

/*
 * Testing RT task to send a stream to a specific smart object.
 * Helpful to perform assessment of the wireless transmission.
 */
static void soo_stream_task_fn(void *args) {

	sl_desc_t *sl_desc;
	int i;

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	sl_desc = sl_register(SL_REQ_PEER, SL_IF_WLAN, SL_MODE_UNIBROAD);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	sl_desc = sl_register(SL_REQ_PEER, SL_IF_ETH, SL_MODE_UNIBROAD);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	for (i = 0; i < BUFFER_SIZE; i++)
		buffer[i] = i;

	rtdm_sl_set_recv_callback(sl_desc, soo_stream_recv);

	soo_sysfs_register(stream_count, stream_count_read, NULL);
	soo_sysfs_register(neighbours, neighbours_read, NULL);

	while (true) {
#if 1
		if (discovery_neighbour_count() > 0) {
			lprintk("*** sending buffer ****\n");
			rtdm_sl_send(sl_desc, buffer, BUFFER_SIZE, get_null_agencyUID(), 10);
			rtdm_sl_send(sl_desc, NULL, 0, get_null_agencyUID(), 10);
			lprintk("*** End. ***\n");
		}
#endif
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
