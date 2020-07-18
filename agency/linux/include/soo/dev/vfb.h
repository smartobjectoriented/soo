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

#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VFB_NAME   "vfb"
#define VFB_PREFIX "[" VFB_NAME "-back] "

/* Arbitrary values. On rpi4, the LEDs are considered as a 8x8 fb. */
#define MIN_FB_HRES 640
#define MIN_FB_VRES 480

/* General structure for this virtual device (backend side). */
typedef struct {
	vdevback_t vdevback;
} vfb_t;

/* Represent a domain framebuffer. */
struct vfb_fb {

	/* Domain id associated with the framebuffer. */
	domid_t domid;

	/* Size of the framebuffer. */
	uint32_t size;

	/* Physical address of the framebuffer. */
	uint64_t paddr;

	/* Virtual address of the framebuffer. */
	uint32_t vaddr;

	/* Memory area allocated for the framebuffer (mapped to ME memory). */
	struct vm_struct *area;

	/* Data used to do the grantref mapping. */
	struct gnttab_map_grant_ref *op;
};

void vfb_reconfig(domid_t);
struct vfb_fb *vfb_get_fefb(domid_t);
domid_t vfb_current_fefb(void);
void vfb_register_callback(void (*cb)(struct vfb_fb *fb));

#endif /* VFB_H */
