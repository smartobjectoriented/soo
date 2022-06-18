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

static uint8_t mac_null_addr[ETH_ALEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

typedef struct {
	plugin_desc_t plugin_bt_desc;
} soo_plugin_bt_t;

/* Interface with the RFCOMM subsystem */
int rfcomm_tty_write_sl_plugin(const unsigned char *buf, int count);

void plugin_bt_tx(sl_desc_t *sl_desc, void *data, size_t size) {

	/* The packet has to be forwarded to the RFCOMM layer */
	rfcomm_tty_write_sl_plugin(data, size);
}


/**
 * Receive a packet from the Bluetooth interface.
 * For example, called from net/bluetooth/rfcomm/tty.c
 */
void plugin_bt_rx(struct sk_buff *skb) {
	soo_plugin_bt_t *soo_plugin_bt;

	soo_plugin_bt = container_of(current_soo_plugin->__intf[SL_IF_BT], soo_plugin_bt_t, plugin_bt_desc);

	plugin_rx(&soo_plugin_bt->plugin_bt_desc,
		  get_sl_req_type_from_protocol(ntohs(skb->protocol)), mac_null_addr, skb->data, skb->len);

	kfree_skb(skb);
}

/**
 * This function must be executed in the non-RT domain.
 */
void plugin_bt_init(void) {
	soo_plugin_bt_t *soo_plugin_bt;

	lprintk("SOOlink: Bluetooth (BT) Plugin init...\n");

	soo_plugin_bt = (soo_plugin_bt_t *) kzalloc(sizeof(soo_plugin_bt_t), GFP_KERNEL);
	BUG_ON(!soo_plugin_bt);

	soo_plugin_bt->plugin_bt_desc.tx_callback = plugin_bt_tx;
	soo_plugin_bt->plugin_bt_desc.if_type = SL_IF_BT;

	transceiver_plugin_register(&soo_plugin_bt->plugin_bt_desc);
}
