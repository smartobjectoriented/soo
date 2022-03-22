/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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
#include <delay.h>
#include <timer.h>
#include <heap.h>
#include <memory.h>

#include <device/irq.h>

#include <soo/avz.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/logbool.h>
#include <soo/evtchn.h>

#include <soo/dev/vwagoled.h>

#include <me/wagoled.h>


int app_thread_main(void *args) {

	int ids [] = {1, 2, 3, 4, 5, 6};
	int size = 6;
	wago_cmd_t status = LED_ON;

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk("Welcome to WAGO led ME\n");

	while (1) {

		msleep(500);

		if (vwagoled_set(ids, size, status) < 0) {
			DBG("Error vwagoled set\n");
			continue;
		}

		status = (status == LED_ON) ? LED_OFF :LED_ON;
	}

	return 0;
}
