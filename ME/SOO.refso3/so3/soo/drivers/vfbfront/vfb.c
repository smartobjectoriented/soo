/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <heap.h>
#include <mutex.h>
#include <delay.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vfb.h>

vfb_t vfb;

static bool thread_created = false;

irq_return_t vfb_interrupt(int irq, void *dev_id)
{
	vfb_response_t *vfb_response;

	if (!vfb_is_connected())
		return IRQ_COMPLETED;

	DBG("%s, %d\n", __func__, ME_domID());

	vfb_response = vfb_ring_response(&vfb.ring);

	/* Do something... */

	return IRQ_COMPLETED;
}

/*
 * The following function is given as an example.
 *
 */
void vfb_generate_request(char *buffer)
{
	vfb_request_t *ring_req;

	vfb_start();

	/*
	 * Try to generate a new request to the backend
	 */

	ring_req = vfb_ring_request(&vfb.ring);
	if (ring_req) {
		memcpy(ring_req->buffer, buffer, VFB_PACKET_SIZE);

		vfb_ring_request_ready(&vfb.ring);

		notify_remote_via_irq(vfb.irq);
	}

	vfb_end();
}

void vfb_probe(void)
{
	DBG("vfb init ok\n");
	write_vbstore();

	DBG0("[" VFB_NAME "] Frontend probe\n");
}

/* At this point, the FE is not connected. */
void vfb_reconfiguring(void)
{
	DBG0("[" VFB_NAME "] Frontend reconfiguring\n");
}

void vfb_shutdown(void)
{
	DBG0("[" VFB_NAME "] Frontend shutdown\n");
}

void vfb_closed(void)
{
	DBG0("[" VFB_NAME "] Frontend close\n");
}

void vfb_suspend(void)
{
	DBG0("[" VFB_NAME "] Frontend suspend\n");
}

void vfb_resume(void)
{
	DBG0("[" VFB_NAME "] Frontend resume\n");
}

int vfb_notify_fn(void *arg)
{
	while (1) {
		msleep(50);

		vfb_start();

		/* Make sure the backend is connected and ready for interactions. */

		notify_remote_via_irq(vfb.irq);

		vfb_end();
	}

	return 0;
}

void vfb_connected(void)
{
	DBG0("[" VFB_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vfb.irq);

	if (!thread_created) {
		thread_created = true;
#if 1
		kernel_thread(vfb_notify_fn, "notify_th", NULL, 0);
#endif
	}
}

void write_vbstore(void)
{
	struct vbus_transaction vbt;
	//local_irq_enable();
	vbus_transaction_start(&vbt);
	vbus_write(vbt, "backend/vfb/fb-ph-addr", "value", "try2");
	vbus_transaction_end(vbt);
	//local_irq_disable();
}

static int vfb_init(dev_t *dev)
{
	vfb_vbus_init();


	return 0;
}

REGISTER_DRIVER_POSTCORE("vfb,frontend", vfb_init);
