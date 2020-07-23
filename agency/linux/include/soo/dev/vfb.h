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
#include <linux/fb.h>

#define VFB_NAME   "vfb"
#define VFB_PREFIX "[" VFB_NAME "-back] "

#define DOMFB_COUNT 8

/* Arbitrary values. On rpi4, the LEDs are considered as an 8x8 fb. */
#define MIN_FB_HRES 640
#define MIN_FB_VRES 480

/* General structure for this virtual device (backend side). */
typedef struct {
	vdevback_t vdevback;
} vfb_t;

/* Represent a domain framebuffer. */
struct vfb_domfb {
	domid_t id;                      /* Domain id associated with the framebuffer. */
	uint64_t paddr;                  /* Physical address of the framebuffer. */
	char *vaddr;                     /* Virtual address of the framebuffer. */
	struct vm_struct *area;          /* Memory area allocated for the framebuffer (mapped to ME memory). */
	grant_handle_t gnt_handle;       /* Grant handle to do the unmapping. */
};

void vfb_set_active_domfb(domid_t);
struct vfb_domfb *vfb_get_domfb(domid_t);
void vfb_set_callback_new_domfb(void (*cb)(struct vfb_domfb *, struct fb_info *));
void vfb_set_callback_rm_domfb(void (*cb)(domid_t));

#endif /* VFB_H */
