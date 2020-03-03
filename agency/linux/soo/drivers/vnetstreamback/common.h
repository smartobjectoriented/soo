/*
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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
#include <soo/uapi/avz.h>
#include <soo/vbus.h>

#include <soo/dev/vnetstream.h>

#include <xenomai/rtdm/driver.h>

typedef struct {

	vnetstream_cmd_back_ring_t ring;
	rtdm_irq_t	irq_handle;

} vnetstream_cmd_ring_t;

typedef struct {

	vnetstream_tx_back_ring_t ring;
	rtdm_irq_t	irq_handle;

} vnetstream_tx_ring_t;

typedef struct {

	vnetstream_rx_back_ring_t ring;
	rtdm_irq_t	irq_handle;

} vnetstream_rx_ring_t;

typedef struct {
	char		*data;
	unsigned int	pfn;
} vnetstream_shared_buffer_t;

typedef struct {
	struct vbus_device *vdev[MAX_DOMAINS];
	
	vnetstream_cmd_ring_t	cmd_rings[MAX_DOMAINS];
	vnetstream_tx_ring_t	tx_rings[MAX_DOMAINS];
	vnetstream_rx_ring_t	rx_rings[MAX_DOMAINS];

	vnetstream_shared_buffer_t	txrx_buffers[MAX_DOMAINS];

} vnetstream_t;

extern vnetstream_t vnetstream;

extern size_t vnetstream_packet_size;

/* ISRs associated to the rings */
int vnetstream_cmd_interrupt(rtdm_irq_t *handle);
int vnetstream_tx_interrupt(rtdm_irq_t *handle);
int vnetstream_rx_interrupt(rtdm_irq_t *handle);

/* Shared buffer setup */
void vnetstream_setup_shared_buffer(struct vbus_device *dev);

/* State management */
void vnetstream_probe(struct vbus_device *dev);
void vnetstream_close(struct vbus_device *dev);
void vnetstream_suspend(struct vbus_device *dev);
void vnetstream_resume(struct vbus_device *dev);
void vnetstream_connected(struct vbus_device *dev);
void vnetstream_reconfigured(struct vbus_device *dev);
void vnetstream_shutdown(struct vbus_device *dev);

void vnetstream_vbus_init(void);

bool vnetstream_start(domid_t domid);
void vnetstream_end(domid_t domid);
bool vnetstream_is_connected(domid_t domid);

