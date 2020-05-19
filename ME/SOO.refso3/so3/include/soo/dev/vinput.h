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

extern void input_event(struct input_dev *dev, unsigned int type, unsigned int code, int value);
bool kbd_present(void);

#endif /* VINPUT_H */
