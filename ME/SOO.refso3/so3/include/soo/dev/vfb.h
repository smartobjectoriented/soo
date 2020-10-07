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

#include <linux/fb.h> /* struct fb_bitfield */

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
	unsigned long addr;
	unsigned long vaddr;
	unsigned int len;
} vfb_info_t;

int vfb_get_params(vfb_hw_params_t *hw_params);

#endif /* VFB_H */
