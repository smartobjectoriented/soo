/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h> /* GFP Flags for kmalloc */

#include <linux/unistd.h>
#include <linux/errno.h>
#include <linux/uio.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/kthread.h>
#include <linux/rwsem.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/sched.h>

#include <linux/ipipe_base.h>

#include <asm/ipipe_base.h>

#include <asm/memory.h>

#include <soo/vbus.h>
#include <soo/vbstore.h>
#include <soo/hypervisor.h>
#include <soo/paging.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/event_channel.h>

#include <soo/gnttab.h>
#include <soo/grant_table.h>
#include <soo/vbus.h>
#include <soo/evtchn.h>
#include <soo/uapi/debug.h>

#define IRQF_DISABLED	0

/* Interfaces/addresses to vbstore and event channel for each domain */

/*
 * vbstore_intf[] contains the evtchn and re-mapped virtual address (no-cached)
 * while __vbstore_vaddr[] stores the *real* (linear) virtual address which
 * can be worked with virt_to_mfn()
 */
struct vbstore_domain_interface *vbstore_intf[MAX_DOMAINS];
void *__vbstore_vaddr[MAX_DOMAINS];

static struct list_head notify_list;

typedef struct {
	struct list_head list;
	vbstore_intf_t *intf;  /* Domain source identification */
	struct vbs_node *notify_node;

	uint32_t transactionID;

} notify_item_t;

/*
 *  Read / write primitives to access the ring. Work on a specific ring
 *  retrieved through the given event channel.
 */
static void vbs_s_write(vbstore_intf_t *intf, const void *data, unsigned len) {

	VBSTORE_RING_IDX prod;
	volatile char *dst;

	/* Read indexes, then verify. */
	prod = intf->rsp_pvt;

	/* Check if we are at the end of the ring, i.e. there is no place for len bytes */
	if (prod + len >= VBSTORE_RING_SIZE) {
		prod = intf->rsp_pvt = 0;

		if (prod + len > intf->rsp_cons)
			BUG();
	}

#warning assert that the ring is not full

	dst = &intf->rsp[prod];

	/* Must write data /after/ reading the consumer index. */
	mb();

	memcpy((void *) dst, data, len);

	intf->rsp_pvt += len;

	/* Other side must not see new producer until data is there. */
	mb();

}

static void vbs_s_read(vbstore_intf_t *intf, void *data, unsigned len) {
	VBSTORE_RING_IDX cons;
	volatile const char *src;

	/* Read indexes, then verify. */
	cons = intf->req_cons;

	if (cons + len >= VBSTORE_RING_SIZE)
		cons = intf->req_cons = 0;

	src = &intf->req[cons];

	/* Must read data /after/ reading the producer index. */
	mb();

	memcpy(data, (void *) src, len);

	intf->req_cons += len;

	/* Other side must not see free space until we've copied out */
	mb();
}

/*
 * Send the response to the request
 */
static void send_reply(volatile vbstore_intf_t *intf, vbus_msg_t *reply) {

	/* vbs_s_write() performs a copy of the msg */
	vbs_s_write(intf, reply, sizeof(vbus_msg_t));

	if (reply->len > 0)
		vbs_s_write(intf, reply->payload, reply->len);

	intf->rsp_prod = intf->rsp_pvt;

	mb();

	/* Implies mb(): other side will see the updated producer. */

        DBG("   VBstore replying with msg type: %d for msg: %d notifying on remote evtchn: %d ...\n", reply->type, reply->id, intf->revtchn);

     	notify_remote_via_evtchn(intf->revtchn);
}

/*
 * Send a WATCH_EVENT to all of the observers watching the modified path and
 * value.
 */
void vbs_notify_watchers(vbus_msg_t msg, struct vbs_node *node) {
	const struct vbs_node *current_node = node;
	struct vbs_watcher *watcher;
	char path[VBS_KEY_LENGTH];

	/* Prepare a watch event vbus_msg message */
	msg.type = VBS_WATCH_EVENT;

	/*
	 * We walk through the whole vbstore tree to notify each parent
	 * from the node when necessary.
	 */
	while (current_node != NULL) {

		list_for_each_entry(watcher, &current_node->watchers, list) {

			vbs_get_absolute_path(node, path);

			DBG("SENDING WATCH_EVENT %s to watcher revtchn: %d (intf: %p)\n", path, watcher->intf->revtchn, watcher->intf);

			msg.payload = path;
			msg.len = strlen(path) + 1;

			send_reply(watcher->intf, &msg);
		}

		current_node = current_node->parent;
	}
}

/*
 * Process message received in vbstore from a domain.
 *
 * A list of modified properties in vbstore is preserved in a dynamic list so that they can be sent
 * to the watchers at the end of the transaction (only works with a vbus transaction).
 *
 * This function is executed with IRQs remaining disabled.
 *
 */
 irqreturn_t vbstore_interrupt(int irq, void *__vbstore_intf) {
	volatile vbstore_intf_t *intf = (vbstore_intf_t *) __vbstore_intf;
	vbus_msg_t *msg;
	struct vbs_node *node;
	int length;
	static char buffer[VBSTORE_RING_SIZE];
	bool found;

	notify_item_t *notify_item, *tmp_notify_ptr;
	char *payload, *reply = buffer, *value;

	DBG("vbstore_interrupt: IRQ : %d %p\n", irq, intf);

	BUG_ON(!hard_irqs_disabled());

	while (intf->req_cons != intf->req_prod) {

		length = 0;

		/* msg & payload are the received message to be processed */

		msg = kzalloc(sizeof(vbus_msg_t), GFP_ATOMIC);
		if (msg == NULL) {
			lprintk("%s - line %d: allocation of kmalloc() failed\n", __func__, __LINE__);
			BUG();
		}

		vbs_s_read(intf, msg, sizeof(vbus_msg_t));

		DBG("CPU: %d got msg: %d\n", smp_processor_id(), msg->id);

		payload = NULL;
		if (msg->len > 0) {
			/* Now, the new message got the ID and the transactionID */
			payload = kzalloc(msg->len, GFP_ATOMIC);
			if (payload == NULL) {
				lprintk("%s: failure returned by kmalloc at line %d\n", __func__, __LINE__);
				BUG();
			}

			vbs_s_read(intf, payload, msg->len);
		}

		switch (msg->type) {

		case VBS_DIRECTORY:
			BUG_ON(payload == NULL);
			length = vbs_store_readdir(payload, reply, VBSTORE_RING_SIZE);

			DBG("VBS_DIRECTORY(%s) reply: %s length %i\n", payload, reply, length);
			break;

		case VBS_DIRECTORY_EXISTS:
			BUG_ON(payload == NULL);
			length = vbs_store_dir_exists(payload, reply);

			DBG("VBS_DIRECTORY_EXISTS(%s) reply: %s length %i\n", payload, reply, length);
			break;

		case VBS_MKDIR:
			BUG_ON(payload == NULL);
			length = vbs_store_readdir(payload, reply, VBSTORE_RING_SIZE);
			vbs_store_mkdir(payload);

			DBG("VBS_MKDIR(%s)\n", payload);
			break;

		case VBS_RM:
			BUG_ON(payload == NULL);
			vbs_store_rm(payload);
			break;

		case VBS_WATCH:
			BUG_ON(payload == NULL);
			vbs_store_watch(payload, intf);

			DBG("VBS_WATCH on %s\n", payload);
			break;

		case VBS_UNWATCH:
			BUG_ON(payload == NULL);
			vbs_store_unwatch(payload, intf);
			break;

		case VBS_WRITE:
			BUG_ON(payload == NULL);

			/*
			 * We get two strings, the first one contains the node name (key), the second one contains the value.
			 * These two strings are simply appended with '\0' at the end of each string.
			 */
			value = (payload + strlen(payload) + 1);

			/* Perform the write in the vbs store and retrieve associated node for notification. */
			vbs_store_write_notify(payload, value, &node);

			DBG("VBS_WRITE(%s) = %s transactionID = %d\n", payload, value, msg->transactionID);

			if (node != NULL) {

				found = false;
				list_for_each_entry(notify_item, &notify_list, list) {
					if ((notify_item->notify_node == node) && (notify_item->intf == intf) &&
					    ((msg->transactionID == 0) || (notify_item->transactionID == msg->transactionID))) {
						found = true;
						break;
					}
				}

				if (!found) {
					notify_item = kmalloc(sizeof(notify_item_t), GFP_ATOMIC);
					if (notify_item == NULL) {
						lprintk("%s: failure returned by kmalloc at line %d\n", __func__, __LINE__);
						BUG();
					}

					notify_item->notify_node = node;
					notify_item->intf = intf;
					notify_item->transactionID = msg->transactionID;

					list_add_tail(&notify_item->list, &notify_list);
				}
			}

			break;

		case VBS_READ:
			BUG_ON(payload == NULL);
			vbs_store_read(payload, &reply, VBSTORE_RING_SIZE);
			length = strlen(reply) + 1;

			DBG("VBS_READ(%s) '%s' %i\n", payload, reply, length);
			break;



		case VBS_TRANSACTION_END:

			DBG("VBS_TRANSACTION_END: evtchn : %d\n", intf->revtchn);

			/* A Write operation has been done, we need to notify the watchers. */

			list_for_each_entry_safe(notify_item, tmp_notify_ptr, &notify_list, list) {

				if ((notify_item->intf == intf) &&
				    ((msg->transactionID == 0) || (notify_item->transactionID == msg->transactionID))) {

					vbs_notify_watchers(*msg, notify_item->notify_node);

					list_del(&notify_item->list);

					kfree(notify_item);

				}
			}
			break;

		default:
			lprintk("------------------- vbs_%i(%s) Unexpected !!\n", msg->type, payload);
			BUG();
			break;
		}

		msg->payload = reply;
		msg->len = length;

		send_reply(intf, msg);

		/* Reset the buffer and free msg & body used to receive */
		memset(reply, 0, length);

		kfree(msg);
		kfree(payload);

	}

	BUG_ON(!hard_irqs_disabled());

	return IRQ_HANDLED;
 }

 /*
  * DOMCALL_sync_vbstore
  */
 int do_sync_vbstore(void *arg)
 {
	 struct DOMCALL_sync_vbstore_args *args = arg;
	 unsigned int domID = args->vbstore_revtchn; /* a way to pass domID from the hypervisor */

	 args->vbstore_pfn = virt_to_pfn(__vbstore_vaddr[domID]);

	 /*
	  * Get the event channel used on the agency side to notify vbstore events to the ME.
	  * This is used for re-binding the inter-domain event channel in avz.
	  */
	 args->vbstore_revtchn = vbstore_intf[domID]->revtchn;

	 return 0;
 }

 /*
 * VBstore in this file works like the backend with shared rings.
 */
void vbstore_init(void) {
	int res = 0;
	int i;
	void *__vaddr;
	struct evtchn_alloc_unbound alloc_unbound;

	lprintk("... vbstore SOO Agency setting up...\n");

	/* Allocate a vbstore page for each domain */
	for (i = 0; i < MAX_DOMAINS; i++) {

		/* Allocate a shared page for vbstore. We get a first virtual address of a page that will be
		 * used to pass the pfn to the ME during unpause() operation.
		 * However, we will not use this address as the shared page virtual address, but another (remapped)
		 * address in order to have a non-cached page.
		 */
		__vbstore_vaddr[i] = (void *) get_zeroed_page(GFP_KERNEL);
		if (!__vbstore_vaddr[i]) {
			lprintk("get_zeroed_page(GFP_KERNEL) failed.\n");
			BUG();
		}

		/* Make sure the page will not be cached */
		vbstore_intf[i] = (struct vbstore_domain_interface *) paging_remap(virt_to_phys(__vbstore_vaddr[i]), PAGE_SIZE);

	}

	/* Set our local interface to vbstore */
	__intf = (vbstore_intf_t *) vbstore_intf[0];

	/* Set __intf for the RT domain. */
	__intf_rt = (vbstore_intf_t *) vbstore_intf[1];

	/* Allocate a shared page for vbstore. */
	__vaddr = (void *) get_zeroed_page(GFP_KERNEL);
	if (!__vaddr) {
		lprintk("get_zeroed_page(GFP_KERNEL) failed.\n");
		BUG();
	}

	/*
	 * Initialize the list of notification to be propagated belonging
	 * to a transaction.
	 */
	INIT_LIST_HEAD(&notify_list);

	/*
	 * Allocate unbound evtchns which vbstored/vbstore thread can bind to.
	 */

	for (i = 0; i < MAX_DOMAINS; i++) {

		alloc_unbound.dom = DOMID_SELF;
		alloc_unbound.remote_dom = i;

		hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);

		/* Store the allocated unbound evtchn.*/
		DBG("%s: allocating unbound evtchn %d for vbstore shared page on domain: %d (intf = %p)...\n", __func__, alloc_unbound.evtchn, i, vbstore_intf[i]);
		vbstore_intf[i]->revtchn = alloc_unbound.evtchn;
	}

	/* Now, initialize the basic vbstore virtual database */
	vbstorage_agency_init();

	/* Bind the vbstore listening event channels */
	for (i = 0; i < MAX_DOMAINS; i++) {

		/* Bind IRQ used as event channel to discuss with the ME to the vbstore interrupt handler */

		DBG("%s: binding evtchn %d to vbstore_interrupt handler..\n", __func__, vbstore_intf[i]->revtchn);

		res = bind_evtchn_to_virq_handler(vbstore_intf[i]->revtchn, vbstore_interrupt, NULL, IRQF_DISABLED, "vbstore", vbstore_intf[i]);
		if (res < 0) {
			lprintk(KERN_ERR "VBus request virq failed %i\n", res);
			BUG();
		}
	}

	/* Initialize the interface to vbstore. */
	res = vbus_vbstore_init();
	if (res) {
		printk(KERN_WARNING "VBus: Error initializing vbstore comms: %i\n", res);
		BUG();
	}
}


