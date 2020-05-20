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

#include <mutex.h>

#include <device/irq.h>

#include <soo/evtchn.h>
#include <soo/avz.h>

#include <soo/dev/vuihandler.h>

typedef struct {

	struct vbus_device	*dev;

	vuihandler_tx_front_ring_t	tx_ring;
	grant_ref_t	tx_ring_ref;
	grant_handle_t	tx_handle;
	uint32_t	tx_evtchn;
	uint32_t	tx_irq;

	vuihandler_rx_front_ring_t	rx_ring;
	grant_ref_t 	rx_ring_ref;
	grant_handle_t	rx_handle;
	uint32_t	rx_evtchn;
	uint32_t	rx_irq;

	char		*tx_data;
	unsigned int	tx_pfn;

	char		*rx_data;
	unsigned int	rx_pfn;

	struct vbus_watch	app_watch;

} vuihandler_t;

extern vuihandler_t vuihandler;

/* ISRs associated to the rings */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id);
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id);

void vuihandler_app_watch_fn(struct vbus_watch *watch);

/*
 * Interface with the client.
 * These functions must be provided in the applicative part.
 */
void ui_update_app_spid(uint8_t *spid);
void ui_interrupt(char *data, size_t size);

/* Shared buffer setup */
int vuihandler_set_shared_buffer(void *data, size_t size);
int vuihandler_setup_shared_buffer(void);

/* State management */
void vuihandler_probe(void);
void vuihandler_closed(void);
void vuihandler_suspend(void);
void vuihandler_resume(void);
void vuihandler_connected(void);
void vuihandler_reconfiguring(void);
void vuihandler_shutdown(void);

void vuihandler_vbus_init(void);

/* Processing and connected state management */
void vuihandler_start(void);
void vuihandler_end(void);
bool vuihandler_is_connected(void);
