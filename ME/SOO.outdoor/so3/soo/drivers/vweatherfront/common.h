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

#include <soo/dev/vweather.h>

typedef struct {

	bool		connected;
	struct vbus_device  *dev;

	uint32_t	evtchn;
	uint32_t	irq;

	char		*weather_data;
	unsigned int	weather_pfn;

} vweather_t;

extern vweather_t vweather;

/* ISR associated to the notification */
irq_return_t vweather_update_interrupt(int irq, void *dev_id);

/*
 * Interface with the client.
 * This function must be provided in the applicative part.
 */
void weather_data_update_interrupt(void);

/* State management */
void vweather_probe(void);
void vweather_close(void);
void vweather_suspend(void);
void vweather_resume(void);
void vweather_connected(void);
void vweather_reconfiguring(void);
void vweather_shutdown(void);

void vweather_vbus_init(void);

/* Processing and connected state management */
void vweather_start(void);
void vweather_end(void);
bool vweather_is_connected(void);

