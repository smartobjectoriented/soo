/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/core/device_access.h>
#include <soo/core/sysfs.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/transcoder.h>
#include <soo/soolink/coder.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/datalink.h>

/* List of registered requesters */
struct list_head sl_req_list;

/* Preparation of args to be passed in the RT domain */
static sl_send_args_t sl_send_args;
static sl_recv_args_t sl_recv_args;

static spinlock_t send_lock;
static spinlock_t recv_lock;

/*
 * Look for a specific descriptor according to the type of requester
 */
sl_desc_t *find_sl_desc_by_req_type(req_type_t req_type) {
	sl_desc_t *cur;

	list_for_each_entry(cur, &sl_req_list, list) {
		if (cur->req_type == req_type)
			return cur;
	}

	return NULL;
}

/*
 * Register a new requester in Soolink.
 *
 * This function can be called from the non-RT or RT agency domain.
 */
sl_desc_t *sl_register(req_type_t req_type, if_type_t if_type, trans_mode_t trans_mode) {
	sl_desc_t *sl_desc;

	sl_desc = kmalloc(sizeof(sl_desc_t), GFP_ATOMIC);

	if (!sl_desc) {
		lprintk("%s: failed to allocate a new requester...\n", __func__);
		BUG();
	}

	memset(sl_desc, 0, sizeof(sl_desc_t));

	sl_desc->req_type = req_type;
	sl_desc->if_type = if_type;
	sl_desc->trans_mode = trans_mode;
	sl_desc->recv_callback = NULL;

	memcpy(&sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
	memcpy(&sl_desc->agencyUID_from, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

	init_completion(&sl_desc->recv_event);

	list_add_tail(&sl_desc->list, &sl_req_list);

	return sl_desc;
}

/*
 * Unregister a requester
 */
void sl_unregister(sl_desc_t *sl_desc) {
	sl_desc_t *cur;

	list_for_each_entry(cur, &sl_req_list, list)
		if (cur == sl_desc) {
			list_del(&cur->list);
			kfree(cur);

			break;
		}

}

/*
 * Return the number of smart objects detected in the neighborhood.
 */
uint32_t sl_neighbour_count(void) {
	return discovery_neighbour_count();
}


/*
 * Send data over a specific interface configured in the sl_desc descriptor.
 * It is important to note that sending a buffer should be followed by sending a "NULL"
 * since - at the datalink level (transceiver) - we could remain "speaker" over a while
 * and prevent other smart objects belonging to the neighborhood to become a speaker.
 *
 * This function runs in the RT agency domain.
 */
void sl_send(sl_desc_t *sl_desc, void *data, size_t size, agencyUID_t *agencyUID, uint32_t prio) {
	DBG("sl_send: now sending to the coder / size: %d\n", size);

	/* Configure the sl_desc with the various attributes */

	/* According to the transmission mode, we do not want to handle the destination */
	if (sl_desc->trans_mode != SL_MODE_UNIBROAD)
		memcpy(&sl_desc->agencyUID_to, agencyUID, SOO_AGENCY_UID_SIZE);

	sl_desc->prio = prio;

	coder_send(sl_desc, data, size);

	DBG("sl_send: OK.\n");
}

/*
 * Receive data over the interface attached in the sl_desc descriptor
 *
 * This function is synchronous and runs in the non-RT agency domain.
 * So far, it is necessary to have a dedicated DC event for each type of interface provided that
 * the low-level function of the requester will never be called simultaneously. One request after
 * one request must be processed.
 */
int sl_recv(sl_desc_t *sl_desc, void **data) {
	return decoder_recv(sl_desc, data);
}

/*
 * Manage exclusive access over an interface.
 */
void sl_set_exclusive(sl_desc_t *sl_desc, bool active) {
	sl_desc->exclusive = active;
}

bool is_exclusive(sl_desc_t *sl_desc) {
	return sl_desc->exclusive;
}

/*
 * Configure a receive callback function for asynchronous receive from the transcoder functional block (decoder).
 */
void sl_set_recv_callback(sl_desc_t *sl_desc, recv_callback_t recv_fn) {
	sl_desc->recv_callback = recv_fn;
}

/* Forward the call to the Discovery block to enable the discovery process. */
void sl_discovery_start(void) {
	discovery_start();
}

static int soolink_init(void) {
	lprintk("%s: Soolink subsys initializing ...\n", __func__);

	INIT_LIST_HEAD(&sl_req_list);

	spin_lock_init(&send_lock);
	spin_lock_init(&recv_lock);

	/* Initialize the Transcoder block */
	transcoder_init();

	/* Initialize the Transceiver block */
	transceiver_init();

	return 0;
}

subsys_initcall(soolink_init);
