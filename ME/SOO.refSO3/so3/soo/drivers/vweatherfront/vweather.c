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

#if 0
#define DEBUG
#endif

#include <mutex.h>
#include <heap.h>
#include <sync.h>
#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include "common.h"

vweather_t vweather;

static vweather_interrupt_t __update_interrupt = NULL;

vweather_data_t *vweather_get_data(void) {
	return (vweather_data_t *) vweather.weather_data;
}

irq_return_t vweather_update_interrupt(int irq, void *dev_id) {
	if (likely(__update_interrupt != NULL))
		(*__update_interrupt)();

	return IRQ_COMPLETED;
}

void vweather_probe(void) {
	DBG0(VWEATHER_PREFIX "Frontend probe\n");
}

void vweather_suspend(void) {
	DBG0(VWEATHER_PREFIX "Frontend suspend\n");
}

void vweather_resume(void) {
	DBG0(VWEATHER_PREFIX "Frontend resume\n");
}

void vweather_connected(void) {
	DBG0(VWEATHER_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vweather.irq);
}

void vweather_reconfiguring(void) {
	DBG0(VWEATHER_PREFIX "Frontend reconfiguring\n");
}

void vweather_shutdown(void) {
	DBG0(VWEATHER_PREFIX "Frontend shutdown\n");
}

void vweather_close(void) {
	DBG0(VWEATHER_PREFIX "Frontend close\n");
}

void vweather_register_interrupt(vweather_interrupt_t update_interrupt) {
	__update_interrupt = update_interrupt;
}

static int vweather_init(dev_t *dev) {
	vweather_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vweather,frontend", vweather_init);
