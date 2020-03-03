

/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
#include <soo/dev/vfb.h>
#include <soo/uapi/avz.h>
#include <soo/vbus.h>

typedef struct {
	unsigned int fb_pfn;

	unsigned irq;

	vfb_hw_params_t fb_hw;
	vfb_info_t *vfb_info;

	unsigned char *fb;

} fb_data_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {
	struct vbus_device *vdev[MAX_DOMAINS];
	fb_data_t data[MAX_DOMAINS];

	int domfocus;  /* Specify which ME has the focus. -1 means there is no available ME */

} vfb_t;


extern vfb_t vfb;

irqreturn_t vfb_interrupt(int irq, void *dev_id);

void vfb_probe(struct vbus_device *dev);
void vfb_close(struct vbus_device *dev);
void vfb_connected(struct vbus_device *dev);
void vfb_reconfigured(struct vbus_device *dev);
void vfb_shutdown(struct vbus_device *dev);

extern void vfb_vbus_init(void);

bool vfb_start(domid_t domid);
void vfb_end(domid_t domid);
bool vfb_is_connected(domid_t domid);

