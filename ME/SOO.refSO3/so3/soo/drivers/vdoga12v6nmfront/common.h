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

#include <device/irq.h>

#include <soo/evtchn.h>
#include <soo/avz.h>

#include <soo/dev/vdoga12v6nm.h>

typedef struct {

	struct vbus_device	*dev;

	vdoga12v6nm_cmd_front_ring_t cmd_ring;
	grant_ref_t	cmd_ring_ref;
	grant_handle_t	cmd_handle;
	uint32_t	cmd_evtchn;

	uint32_t	cmd_irq;
	uint32_t	up_irq;
	uint32_t	down_irq;

	mutex_t processing_lock;
	uint32_t processing_count;
	struct mutex processing_count_lock;

	volatile bool connected;
	struct completion connected_sync;

} vdoga12v6nm_t;

extern vdoga12v6nm_t vdoga12v6nm;

/* ISRs associated to the ring and notifications */
irq_return_t vdoga12v6nm_cmd_interrupt(int irq, void *dev_id);
irq_return_t vdoga12v6nm_up_interrupt(int irq, void *dev_id);
irq_return_t vdoga12v6nm_down_interrupt(int irq, void *dev_id);

/*
 * Interface with the client.
 * These functions must be provided in the applicative part.
 */
void up_interrupt(void);
void down_interrupt(void);

/* State management */
void vdoga12v6nm_probe(void);
void vdoga12v6nm_closed(void);
void vdoga12v6nm_suspend(void);
void vdoga12v6nm_resume(void);
void vdoga12v6nm_connected(void);
void vdoga12v6nm_reconfiguring(void);
void vdoga12v6nm_shutdown(void);

void vdoga12v6nm_vbus_init(void);

/* Processing and connected state management */
void vdoga12v6nm_start(void);
void vdoga12v6nm_end(void);
bool vdoga12v6nm_is_connected(void);

