/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/delay.h>
#include <linux/of.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <asm/termios.h>
#include <linux/serial_core.h>
#include <soo/dev/vvalve.h>
#include <linux/tty.h>

extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);
extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern void uart_do_close(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);
extern int uart_do_write(struct tty_struct *tty, const unsigned char *buf, int count);
extern void n_tty_do_flush_buffer(struct tty_struct *tty);

extern ssize_t soo_heat_base_write_cmd(char *buffer, ssize_t len);

typedef struct {

	/* Must be the first field */
	vvalve_t vvalve;

	/* contains data receives from LoRa */
	vvalve_data_t vvalve_data;

	struct tty_struct *tty_uart;

	wait_queue_head_t wait_cmd;

	uint8_t send_cmd;

} vvalve_priv_t;

static struct vbus_device *vvalve_dev = NULL;

void vvalve_notify(struct vbus_device *vdev)
{
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);

	vvalve_ring_response_ready(&vvalve_priv->vvalve.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vvalve_priv->vvalve.irq);
}

static int lora_monitor_fn(void *args) {

	vvalve_priv_t *vvalve_priv = (vvalve_priv_t *)args;


	printk("%s LoRa Thread is running.... \n", VVALVE_PREFIX);

	while(true) {

		printk("%s Waiting for valve cmd...\n", VVALVE_PREFIX);

		if (!vvalve_priv->send_cmd) {

			wait_event_interruptible(vvalve_priv->wait_cmd, vvalve_priv->send_cmd);
		}

		printk("%s Send valve cmd ! \n", VVALVE_PREFIX);
		soo_heat_base_write_cmd(vvalve_priv->vvalve_data.cmd_valve, CMD_DATA_SIZE+2);

		vvalve_priv->send_cmd = 0;
	}

	return 0;
}

irqreturn_t vvalve_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);
	vvalve_request_t *ring_req;
	vvalve_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vvalve_get_ring_request(&vvalve_priv->vvalve.ring)) != NULL) {

		ring_rsp = vvalve_new_ring_response(&vvalve_priv->vvalve.ring);

		memcpy(vvalve_priv->vvalve_data.cmd_valve, ring_req->buffer, CMD_DATA_SIZE+2);

		// vvalve_ring_response_ready(&vvalve_priv->vvalve.ring);
		// notify_remote_via_virq(vvalve_priv->vvalve.irq);
	}

	vvalve_priv->send_cmd = 1;
	wake_up_interruptible(&vvalve_priv->wait_cmd);

	return IRQ_HANDLED;
}

void vvalve_probe(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv;

	printk("%s BACKEND PROBE CALLED\n", VVALVE_PREFIX);

	vvalve_priv = kzalloc(sizeof(vvalve_priv_t), GFP_ATOMIC);
	BUG_ON(!vvalve_priv);

	dev_set_drvdata(&vdev->dev, vvalve_priv);

	vvalve_priv->tty_uart = NULL;

	/* wait data before send command */
	vvalve_priv->send_cmd = 0;

	/* init waitqueue to sleep lora thread until
	   receive cmd from frontend */
	init_waitqueue_head(&vvalve_priv->wait_cmd);

	vvalve_dev = vdev;

	/* TEST */
	memcpy(vvalve_priv->vvalve_data.cmd_valve, "00012100\r\n", CMD_DATA_SIZE+2);

#if 1
	kthread_run(lora_monitor_fn, (void *)vvalve_priv, "lora_monitor");
#endif


	DBG(VVALVE_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vvalve_remove(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vvalve structure for %s\n", __func__,vdev->nodename);

	if(vvalve_priv->tty_uart != NULL) {

		tty_kclose(vvalve_priv->tty_uart);
	}
	
	kfree(vvalve_priv);
}


void vvalve_close(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);

	DBG(VVALVE_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */
	if(vvalve_priv->tty_uart != NULL) {

		tty_kclose(vvalve_priv->tty_uart);
	}


	BACK_RING_INIT(&vvalve_priv->vvalve.ring, (&vvalve_priv->vvalve.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vvalve_priv->vvalve.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vvalve_priv->vvalve.ring.sring);
	vvalve_priv->vvalve.ring.sring = NULL;
}

void vvalve_suspend(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vvalve_resume(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vvalve_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vvalve_sring_t *sring;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);

	DBG(VVALVE_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vvalve_priv->vvalve.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vvalve_interrupt, NULL, 0, VVALVE_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vvalve_priv->vvalve.irq = res;
}

void vvalve_connected(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

#if 0
/*
 * Testing code to analyze the behaviour of the ME during pre-suspend operations.
 */
int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vvalve_start(i))
				continue;

			vvalve_ring_response_ready()

			vvalve_notify(i);

			vvalve_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vvalvedrv = {
	.probe = vvalve_probe,
	.remove = vvalve_remove,
	.close = vvalve_close,
	.connected = vvalve_connected,
	.reconfigured = vvalve_reconfigured,
	.resume = vvalve_resume,
	.suspend = vvalve_suspend
};

int vvalve_init(void) {
	struct device_node *np;

#if 0
	vvalve_priv_t *vvalve_priv;
	vvalve_priv = kzalloc(sizeof(vvalve_priv_t), GFP_ATOMIC);
#endif


	/* TODO: Change to vvalve -> edit DTS*/
	np = of_find_compatible_node(NULL, NULL, "vvalve,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(lora_monitor_fn_te, NULL, "lora_monitor");
#endif

	printk("[ %s ] BACKEND INIT CALLED", VVALVE_NAME);

	vdevback_init(VVALVE_NAME, &vvalvedrv);

	return 0;
}

device_initcall(vvalve_init);
