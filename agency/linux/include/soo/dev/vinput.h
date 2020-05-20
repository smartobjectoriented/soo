/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VINPUT_H
#define VINPUT_H

#include <linux/input.h>

#include <soo/ring.h>
#include <soo/grant_table.h>

struct vinput_request {
  uint32_t cmd;  /* Not used right now */

};

struct vinput_response {
	unsigned int type;
	unsigned int code;
	int value;
};

/*
 * Generate vinput ring structures and types.
 */
DEFINE_RING_TYPES(vinput, struct vinput_request, struct vinput_response);

/* Bridging with the Linux input subsystem */

extern struct hid_device *__hiddev;
int vinput_pass_event(struct input_dev *dev, unsigned int type, unsigned int code, int value);
void vinput_connect(void);
void vinput_disconnect(void);

/* Olimex keyboards values */
#define OLIMEX_KBD_VENDOR_ID			0x1220
#define OLIMEX_KBD_PRODUCT_ID			0x0008

extern int vinput_vbus_init(void);

typedef struct {
	struct vbus_device *dev;

	vinput_back_ring_t ring;
	uint32_t evtchn;
	unsigned int irq;
} vinput_ring_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {
	atomic_t refcnt;
	wait_queue_head_t waiting_to_free;

	vinput_ring_t rings[MAX_ME_DOMAINS];

	int domfocus;

} vinput_t;

extern vinput_t vinput;
irqreturn_t vinput_interrupt(int irq, void *dev_id);

int vinput_subsys_init(struct vbus_device *dev);
int vinput_subsys_enable(struct vbus_device *dev);
void vinput_subsys_remove(struct vbus_device *dev);
void vinput_subsys_disable(struct vbus_device *dev);

#define DRV_PFX "vinput:"
#define DPRINTK(fmt, args...)	pr_debug(DRV_PFX "(%s:%d) " fmt ".\n",	__func__, __LINE__, ##args)

#endif /* VINPUT_H */
