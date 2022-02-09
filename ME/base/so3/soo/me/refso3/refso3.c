/*
 * Copyright (C) 2016-2020 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <me/refso3.h>

/**
 * This ME is used as a reference to develop new mobile entities.
 *
 * The ME does the following:
 *
 * - At its origin, the ME displays a letter in a periodic thread.
 *
 * - The letter is incremented (alphabet ordering) each time all "known" smart objects
 *   have been visited once by the current letter (issued from the origin host).
 *
 * - There is no resident ME except in the origin host.
 */
int app_thread_main(void *args) {

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	sh_refso3->cur_letter = 'A';

	while (1) {

		msleep(500);

		/* Simply display the current letter which is incremented each time a ME comes back */
		lprintk("(%d)",  ME_domID());
		printk("%c ", sh_refso3->cur_letter);

	}

	return 0;
}
