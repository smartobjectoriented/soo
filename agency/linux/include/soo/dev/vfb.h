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

#ifndef VFB_H
#define VFB_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <linux/fb.h> /* struct fb_bitfield */

#define VFB_NAME		"vfb"
#define VFB_PREFIX		"[" VFB_NAME "] "

typedef struct {
	int32_t width;
	int32_t height;
	int32_t depth;    /* bits_per_pixel */

	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency */

	uint32_t line_length;

	unsigned int fb_mem_len;

} vfb_hw_params_t;

/* Additional FB event types to be used by standard notifiers */
/* MUST NOT OVERLAP WITH FB_EVENT_xxx from linux/fb.h */
#define VFB_EVENT_DOM_REGISTER	0x20
#define VFB_EVENT_DOM_SWITCH	0x21

typedef struct {
	uint16_t domid;
	unsigned long paddr;
	unsigned int len;

	struct fb_var_screeninfo var;
} vfb_info_t;

int vfb_get_params(vfb_hw_params_t *hw_params);

int vfb_switch_domain(domid_t dom);

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


#endif /* VFB_H */
