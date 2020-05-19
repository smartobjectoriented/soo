#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <soo/soolink/soolink.h>

#if 0
#define DEBUG
#endif


#include <soo/soolink/datalink/bluetooth.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

static bool bluetooth_ready_to_send(sl_desc_t *sl_desc) {
	DBG("IN %s\n", __func__);
	return true;
}

static int bluetooth_xmit(sl_desc_t *sl_desc, void *packet_ptr, size_t size, bool completed) {
	transceiver_packet_t *packet;
	int ret = 0;
	DBG("IN %s\n", __func__);

	return ret;
}

/**
 * This function is called when a TX request is being issued.
 * The call is made by the Sender.
 */
static int bluetooth_request_xmit(sl_desc_t *sl_desc) {
	DBG("IN %s\n", __func__);

	return 0;
}


static int pkt_cnt = 0;
static int cur_size = 0;
static void *buffer;



typedef struct bt_sl_buf {
	void *buffer;
	size_t size;
} bt_sl_buf_t;

#define NB_BUF 3

static bt_sl_buf_t buffers[NB_BUF];

static int cur_buf_idx = 0;

/**
 * WIP
 */
void bluetooth_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet_ptr, size_t size) {
	DBG("IN %s\n", __func__);
	int i;
#if 0
	receiver_rx(sl_desc, plugin_desc, packet_ptr, size);
	return;
#endif
	
	memcpy(buffers[cur_buf_idx++].buffer, packet_ptr, size);
	buffers[cur_buf_idx].size = 0;

	if (cur_buf_idx == 3) {
		cur_buf_idx = 0;

		for (i = 0; i < 3; ++i) {
			receiver_rx(sl_desc, plugin_desc, buffers[i].buffer, buffers[i].size);
		}
	}
}

/**
 * Callbacks of the bluetooth protocol
 */
static datalink_proto_desc_t bluetooth_proto = {
	.ready_to_send = bluetooth_ready_to_send,
	.xmit_callback = bluetooth_xmit,
	.rx_callback = bluetooth_rx,
	.request_xmit_callback = bluetooth_request_xmit,
};

/**
 * Register the bluetooth protocol with the Datalink subsystem. The protocol is associated
 * to the SL_PROTO_bluetooth ID.
 */
static void bluetooth_register(void) {
	datalink_register_protocol(SL_DL_PROTO_BT, &bluetooth_proto);
}

/**
 * Initialization of bluetooth.
 */
void bluetooth_init(void) {
	int i ;
	DBG("bluetooth-datalink initialization\n");


	for (i = 0; i < NB_BUF; ++i) {
		buffers[i].buffer = vmalloc(960);
		if (buffers[i].buffer == NULL) {
			printk("vmalloc failed in %s\n", __func__);
			BUG();
		} 
		buffers[i].size = 0;
	}

	bluetooth_register();
}