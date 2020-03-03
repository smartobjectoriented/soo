/*
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

#include <linux/irqreturn.h>
#include <soo/evtchn.h>
#include <soo/uapi/avz.h>
#include <soo/vbus.h>

#include <soo/dev/vuihandler.h>

#if defined(CONFIG_BT_RFCOMM)

#include <asm/signal.h>

void rfcomm_send_sigterm(void);

#endif /* CONFIG_BT_RFCOMM */

typedef struct {

	struct vbus_device  *dev;

	vuihandler_tx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_tx_ring_t;

typedef struct {

	struct vbus_device  *dev;

	vuihandler_rx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_rx_ring_t;

typedef struct {
	char		*data;
	unsigned int	pfn;

} vuihandler_shared_buffer_t;

typedef struct {

	vuihandler_tx_ring_t	tx_rings[MAX_DOMAINS];
	vuihandler_rx_ring_t	rx_rings[MAX_DOMAINS];

	vuihandler_shared_buffer_t	tx_buffers[MAX_DOMAINS];
	vuihandler_shared_buffer_t	rx_buffers[MAX_DOMAINS];

	/* Table that holds the SPID of the ME whose frontends are connected */
	uint8_t spid[MAX_DOMAINS][SPID_SIZE];

	struct vbus_device  *vdev[MAX_DOMAINS];

} vuihandler_t;

extern vuihandler_t vuihandler;

extern uint8_t vuihandler_null_spid[SPID_SIZE];

/* ISRs associated to the rings */
irqreturn_t vuihandler_tx_interrupt(int irq, void *dev_id);
irqreturn_t vuihandler_rx_interrupt(int irq, void *dev_id);

void vuihandler_update_spid_vbstore(uint8_t spid[SPID_SIZE]);

/* State management */
void vuihandler_probe(struct vbus_device *dev);
void vuihandler_close(struct vbus_device *dev);
void vuihandler_suspend(struct vbus_device *dev);
void vuihandler_resume(struct vbus_device *dev);
void vuihandler_connected(struct vbus_device *dev);
void vuihandler_reconfigured(struct vbus_device *dev);
void vuihandler_shutdown(struct vbus_device *dev);

void vuihandler_vbus_init(void);

bool vuihandler_start(domid_t domid);
void vuihandler_end(domid_t domid);
bool vuihandler_is_connected(domid_t domid);

