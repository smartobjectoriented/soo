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

#include <me/wagoled.h>

/**
 * This ME does nothing particular. It is aimed at giving a template to develop
 * a new ME.
 *
 * Please, have a look at the SOO.ledctrl which is an example of ME intended to
 * pilot LEDs on the Sense HAT extension.
 *
 * Note that SOO.refso3 can be configured with a rootfs (ramfs) which contains
 * small applications like a shell and the LVGL demo application.
 *
 */
int app_thread_main(void *args) {

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	sh_wagoled->cur_letter = 'A';

	while (1) {

		msleep(500);

		lprintk("(%d)",  ME_domID());
		printk("%c ", sh_wagoled->cur_letter);

	}

	return 0;
}
