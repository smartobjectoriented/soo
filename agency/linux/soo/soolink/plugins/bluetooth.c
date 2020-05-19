/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/vbus.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/plugin.h>
#include <soo/soolink/plugin/bluetooth.h>

#include <soo/core/device_access.h>

static spinlock_t send_lock;
static spinlock_t recv_lock;

static volatile plugin_send_args_t plugin_send_args;
static volatile plugin_recv_args_t plugin_recv_args;

#if defined(CONFIG_BT_RFCOMM)
/* Interface with the RFCOMM subsystem */
int rfcomm_tty_write_sl_plugin(const unsigned char *buf, int count);
#endif /* CONFIG_BT_RFCOMM */

/**
 * Send a packet on the Bluetooth interface.
 * This function has to be called in a non-RT context.
 */
static void plugin_bluetooth_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {
	/* Discard Iamasoo (Discovery) beacons */
	if (unlikely(sl_desc->req_type == SL_REQ_DISCOVERY))
		return ;

	spin_lock(&send_lock);

	plugin_send_args.sl_desc = sl_desc;
	plugin_send_args.data = data;
	plugin_send_args.size = size;

	do_sync_dom(DOMID_AGENCY, DC_PLUGIN_BLUETOOTH_SEND);
}

void propagate_plugin_bluetooth_send(void) {
	plugin_send_args_t __plugin_send_args;

	__plugin_send_args = plugin_send_args;

	spin_unlock(&send_lock);

	/* The packet has to be forwarded to the RFCOMM layer */
	rfcomm_tty_write_sl_plugin(__plugin_send_args.data, __plugin_send_args.size);
}

int propagate_plugin_bluetooth_send_fn(void *args) {
	propagate_plugin_bluetooth_send();

	return 0;
}

static plugin_desc_t plugin_bluetooth_desc = {
	.tx_callback	= plugin_bluetooth_tx,
	.if_type	= SL_IF_BT
};

/**
 * Receive a packet from the Bluetooth interface.
 * This function has to be called in a non-RT context.
 */
void sl_plugin_bluetooth_rx(struct sk_buff *skb) {
	req_type_t req_type;

	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	spin_lock(&recv_lock);

	plugin_recv_args.req_type = req_type;
	plugin_recv_args.data = skb->data;
	plugin_recv_args.size = skb->len;

	do_sync_dom(DOMID_AGENCY_RT, DC_PLUGIN_BLUETOOTH_RECV);

	/* Do not free the skb here, it is freed in the RFCOMM layer */
}

/**
 * This function has to be called in a realtime context, from the directcomm RT thread.
 */
void rtdm_propagate_sl_plugin_bluetooth_rx(void) {
	plugin_recv_args_t __plugin_recv_args;

	__plugin_recv_args.req_type = plugin_recv_args.req_type;
	__plugin_recv_args.data = plugin_recv_args.data;
	__plugin_recv_args.size = plugin_recv_args.size;
	memcpy(__plugin_recv_args.mac, (void *) plugin_recv_args.mac, ETH_ALEN);

	spin_unlock(&recv_lock);

	plugin_rx(&plugin_bluetooth_desc, __plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size, __plugin_recv_args.mac);
}

static int plugin_bluetooth_init(void) {
	lprintk("Soolink: Bluetooth Plugin init...\n");

	spin_lock_init(&send_lock);
	spin_lock_init(&recv_lock);

	transceiver_plugin_register(&plugin_bluetooth_desc);

	return 0;
}

soolink_plugin_initcall(plugin_bluetooth_init);
