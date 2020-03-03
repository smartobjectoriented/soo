/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2019 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/of.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/dev/vuart.h>

#include "common.h"

vuart_t vuart;

/* Ring protection */
static spinlock_t sendc_lock;

static void print_guest(uint8_t ch) {
	/* Avoiding to use printk() / lprintk() is the direct path to the UART */

	lprintk("%c", ch);
}

irqreturn_t vuart_interrupt(int irq, void *dev_id) {
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	RING_IDX i, rp;
	vuart_request_t *ring_req;

	if (!vuart_is_connected(dev->otherend_id))
		return IRQ_HANDLED;

	rp = vuart.rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vuart.rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vuart.rings[dev->otherend_id].ring, i);
		print_guest(ring_req->c);
	}

	vuart.rings[dev->otherend_id].ring.sring->req_cons = i;

	return IRQ_HANDLED;
}

/**
 * Bufferize a char while the backend is not in Connected state.
 */
static void add_buf_char(domid_t domid, char c) {
	vuart.buf_chars[domid][vuart.buf_chars_prod[domid] % MAX_BUF_CHARS] = c;
	vuart.buf_chars_prod[domid]++;
}

static void push_response(domid_t domid, uint8_t ch) {
	vuart_response_t *ring_rsp = RING_GET_RESPONSE(&vuart.rings[domid].ring, vuart.rings[domid].ring.sring->rsp_prod);

	ring_rsp->c = ch;

	dmb();

	vuart.rings[domid].ring.rsp_prod_pvt++;

	RING_PUSH_RESPONSES(&vuart.rings[domid].ring);

	notify_remote_via_virq(vuart.rings[domid].irq);
}

/**
 * This function is called in interrupt context.
 * - If the state is Connected, the character can directly be pushed in the ring.
 * - If the state is not Connected, the character is pushed into a circular buffer that
 *   will be flushed at the next call to the Connected callback.
 */
void me_cons_sendc(domid_t domid, uint8_t ch) {

	if (!vuart_is_connected(domid)) {

		spin_lock(&sendc_lock);
		add_buf_char(domid, ch);
		spin_unlock(&sendc_lock);

		return ;
	}

	push_response(domid, ch);
}

void vuart_probe(struct vbus_device *dev) {
	DBG(VUART_PREFIX " Backend probe: %d\n", dev->otherend_id);
}

void vuart_close(struct vbus_device *dev) {
	DBG(VUART_PREFIX " Backend close: %d\n", dev->otherend_id);

}

void vuart_suspend(struct vbus_device *dev) {
	DBG(VUART_PREFIX " Backend suspend: %d\n", dev->otherend_id);
}

void vuart_resume(struct vbus_device *dev) {
	DBG(VUART_PREFIX " Backend resume: %d\n", dev->otherend_id);

}

void vuart_reconfigured(struct vbus_device *dev) {
	DBG(VUART_PREFIX " Backend reconfigured: %d\n", dev->otherend_id);
}

void vuart_connected(struct vbus_device *dev) {
	int i;

	DBG(VUART_PREFIX " Backend connected: %d\n", dev->otherend_id);

	spin_lock(&sendc_lock);

	for (i = vuart.buf_chars_cons[dev->otherend_id]; i < vuart.buf_chars_prod[dev->otherend_id]; i++) 
		push_response(dev->otherend_id, vuart.buf_chars[dev->otherend_id][i % MAX_BUF_CHARS]);

	vuart.buf_chars_cons[dev->otherend_id] = vuart.buf_chars_prod[dev->otherend_id];

	spin_unlock(&sendc_lock);

	notify_remote_via_virq(vuart.rings[dev->otherend_id].irq);
}

int vuart_init(void) {
	unsigned int i;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "soo,vuart");

	/* Check if DTS has vuart enabled */
	if (!of_device_is_available(np))
		return 0;

	spin_lock_init(&sendc_lock);

	for (i = 0; i < MAX_DOMAINS; i++) {

		memset(vuart.buf_chars[i], 0, MAX_BUF_CHARS);
		vuart.buf_chars_prod[i] = 0;
		vuart.buf_chars_cons[i] = 0;
	}

	vuart_vbus_init();

	return 0;
}

module_init(vuart_init);
