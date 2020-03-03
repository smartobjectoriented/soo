/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/irqreturn.h>
#include <soo/evtchn.h>
#include <soo/uapi/avz.h>
#include <soo/vbus.h>

#include <xenomai/rtdm/driver.h>

#include <soo/dev/vdummyrt.h>

typedef struct {

	vdummyrt_back_ring_t	ring;
	rtdm_irq_t		irq_handle;

} vdummyrt_ring_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {
	struct vbus_device *vdev[MAX_DOMAINS];
	vdummyrt_ring_t rings[MAX_DOMAINS];

} vdummyrt_t;

extern vdummyrt_t vdummyrt;

int vdummyrt_interrupt(rtdm_irq_t *dummy);

void vdummyrt_probe(struct vbus_device *dev);
void vdummyrt_close(struct vbus_device *dev);
void vdummyrt_suspend(struct vbus_device *dev);
void vdummyrt_resume(struct vbus_device *dev);
void vdummyrt_connected(struct vbus_device *dev);
void vdummyrt_reconfigured(struct vbus_device *dev);
void vdummyrt_shutdown(struct vbus_device *dev);

extern void vdummyrt_vbus_init(void);

bool vdummyrt_start(domid_t domid);
void vdummyrt_end(domid_t domid);
bool vdummyrt_is_connected(domid_t domid);

