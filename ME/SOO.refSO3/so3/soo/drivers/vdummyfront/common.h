/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <mutex.h>

#include <device/irq.h>

#include <soo/evtchn.h>
#include <soo/avz.h>

#include <soo/dev/vdummy.h>

typedef struct {

	struct vbus_device  *dev;

	vdummy_front_ring_t ring;
	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;
	uint32_t irq;

} vdummy_t;

extern vdummy_t vdummy;

/* ISR associated to the ring */
irq_return_t vdummy_interrupt(int irq, void *data);

/* State management */
void vdummy_probe(void);
void vdummy_closed(void);
void vdummy_suspend(void);
void vdummy_resume(void);
void vdummy_connected(void);
void vdummy_reconfiguring(void);
void vdummy_shutdown(void);

void vdummy_vbus_init(void);

/* Processing and connected state management */
void vdummy_start(void);
void vdummy_end(void);
bool vdummy_is_connected(void);
