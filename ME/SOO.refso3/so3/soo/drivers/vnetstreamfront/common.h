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

#include <soo/dev/vnetstream.h>

typedef struct {

	struct vbus_device	*dev;

	vnetstream_cmd_front_ring_t cmd_ring;
	grant_ref_t	cmd_ring_ref;
	grant_handle_t	cmd_handle;
	uint32_t	cmd_evtchn;
	uint32_t	cmd_irq;

	vnetstream_tx_front_ring_t tx_ring;
	grant_ref_t	tx_ring_ref;
	grant_handle_t	tx_handle;
	uint32_t	tx_evtchn;
	uint32_t	tx_irq;

	vnetstream_rx_front_ring_t rx_ring;
	grant_ref_t	rx_ring_ref;
	grant_handle_t	rx_handle;
	uint32_t	rx_evtchn;
	uint32_t	rx_irq;

	char		*txrx_data;
	unsigned int	txrx_pfn;

} vnetstream_t;

extern vnetstream_t vnetstream;

/* ISRs associated to the rings */
irq_return_t vnetstream_cmd_interrupt(int irq, void *dev_id);
irq_return_t vnetstream_tx_interrupt(int irq, void *dev_id);
irq_return_t vnetstream_rx_interrupt(int irq, void *dev_id);

/*
 * Interface with the client.
 * This function must be provided in the applicative part.
 */
void recv_interrupt(void *data);

/* Shared buffer setup */
int vnetstream_set_shared_buffer(void *data, size_t size);
int vnetstream_setup_shared_buffer(void);

/* State management */
void vnetstream_probe(void);
void vnetstream_closed(void);
void vnetstream_suspend(void);
void vnetstream_resume(void);
void vnetstream_connected(void);
void vnetstream_reconfiguring(void);
void vnetstream_shutdown(void);

void vnetstream_vbus_init(void);

/* Processing and connected state management */
void vnetstream_start(void);
void vnetstream_end(void);
bool vnetstream_is_connected(void);

