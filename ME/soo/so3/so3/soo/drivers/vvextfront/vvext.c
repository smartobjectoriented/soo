/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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
#include <completion.h>
#include <memory.h>

#include <asm/mmu.h>

#include <uapi/linux/input.h>

#include <device/device.h>
#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/gnttab.h>

#include <soo/dev/vvext.h>

/* Our unique vvext instance. */
static struct vbus_device *__vvext = NULL;

typedef struct {
	vvext_t vvext;
	completion_t reader_wait;

} vvext_priv_t;

static int vvext_read(int fd, void *buffer, int count) {
	vvext_priv_t *vvext_priv;
	vvext_response_t *ring_rsp;
	struct input_event *ie;

	vvext_priv = dev_get_drvdata(devclass_by_fd(fd)->dev);
	
	wait_for_completion(&vvext_priv->reader_wait);

	ring_rsp = vvext_get_ring_response(&vvext_priv->vvext.ring);
	
	ie = (struct input_event *) buffer;
	ie->type = ring_rsp->type;
	ie->code = ring_rsp->code;
	ie->value = ring_rsp->value;

	return sizeof(struct input_event);
}

irq_return_t vvext_interrupt(int irq, void *dev_id) {
	vvext_priv_t *vvext_priv = dev_get_drvdata(((struct vbus_device *) dev_id)->dev);

	DBG("%s, %d\n", __func__, ME_domID());

	complete(&vvext_priv->reader_wait);

	return IRQ_COMPLETED;
}


static int vvext_write(int fd, const void *buffer, int count) {
	vvext_request_t *ring_req;
	vvext_priv_t *vvext_priv;

#if 0
	printk("## fd: %d\n", fd);
	printk("## id: %d\n", devclass_fd_to_id(fd));
	printk("## Enable : %d\n", buffer);
#endif
#if 1
	vvext_priv = dev_get_drvdata(devclass_by_fd(fd)->dev);

	vdevfront_processing_begin(__vvext);

	ring_req = vvext_new_ring_request(&vvext_priv->vvext.ring);

	ring_req->buffer[0] = devclass_fd_to_id(fd);
	ring_req->buffer[1] = *((char *) buffer);

	vvext_ring_request_ready(&vvext_priv->vvext.ring);

	notify_remote_via_virq(vvext_priv->vvext.irq);

	vdevfront_processing_end(__vvext);

#endif
	return count;
}


struct file_operations vvext_fops = {
	.write = vvext_write,
	.read = vvext_read
};

struct devclass vvext_cdev = {
	.class = "led",
	.id_start = 0,
	.id_end = 4,
	.type = VFS_TYPE_DEV_CHAR,
	.fops = &vvext_fops,
};

struct devclass vvext_inputdev = {
	.class = "event",
	.id_start = 1,
	.id_end = 1,
	.type = VFS_TYPE_DEV_CHAR,
	.fops = &vvext_fops,
};

void vvext_probe(struct vbus_device *vdev) {
	unsigned int evtchn;
	vvext_sring_t *sring;
	struct vbus_transaction vbt;
	vvext_priv_t *vvext_priv;

	DBG0("[vvext] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	/* Local instance */
	__vvext = vdev;

	vvext_priv = dev_get_drvdata(vdev->dev);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vvext_priv->vvext.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	vbus_alloc_evtchn(vdev, &evtchn);

	vvext_priv->vvext.irq = bind_evtchn_to_irq_handler(evtchn, vvext_interrupt, NULL, vdev);
	vvext_priv->vvext.evtchn = evtchn;
	
	/* Allocate a shared page for the ring */
	sring = (vvext_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vvext_priv->vvext.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	vvext_priv->vvext.ring_ref = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((addr_t) vvext_priv->vvext.ring.sring)));

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vvext_priv->vvext.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vvext_priv->vvext.evtchn);

	vbus_transaction_end(vbt);

}

void vvext_shutdown(struct vbus_device *vdev) {

	DBG0("[vvext] Frontend shutdown\n");
}

void vvext_closed(struct vbus_device *vdev) {
	vvext_priv_t *vvext_priv = dev_get_drvdata(vdev->dev);

	DBG0("[vvext] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vvext_priv->vvext.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vvext_priv->vvext.ring_ref);
		free_vpage((uint32_t) vvext_priv->vvext.ring.sring);

		vvext_priv->vvext.ring_ref = GRANT_INVALID_REF;
		vvext_priv->vvext.ring.sring = NULL;
	}

	if (vvext_priv->vvext.irq)
		unbind_from_irqhandler(vvext_priv->vvext.irq);

	vvext_priv->vvext.irq = 0;
}

void vvext_connected(struct vbus_device *vdev) {

	DBG0("[vvext] Frontend connected\n");

}

void vvext_suspend(struct vbus_device *vdev) {

	DBG0("[vvext] Frontend suspend\n");
}

void vvext_resume(struct vbus_device *vdev) {

	DBG0("[vvext] Frontend resume\n");
}

vdrvfront_t vvext_drv = {
	.probe = vvext_probe,
	.shutdown = vvext_shutdown,
	.closed = vvext_closed,
	.suspend = vvext_suspend,
	.resume = vvext_resume,
	.connected = vvext_connected
};

static int vvext_init(dev_t *dev, int fdt_offset) {
	vvext_priv_t *vvext_priv;

	vvext_priv = malloc(sizeof(vvext_priv_t));
	BUG_ON(!vvext_priv);

	memset(vvext_priv, 0, sizeof(vvext_priv_t));

	dev_set_drvdata(dev, vvext_priv);

	init_completion(&vvext_priv->reader_wait);

	devclass_register(dev, &vvext_cdev);
	devclass_register(dev, &vvext_inputdev);

	vdevfront_init(VVEXT_NAME, &vvext_drv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vvext,frontend", vvext_init);
