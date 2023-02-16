/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <thread.h>
#include <string.h>

#include <soo/xmlui.h>

#include <soo/dev/viuoc.h>
#include <me/iuoc.h>

void *iuoc_send_cmd(void *args) {
	iuoc_cmd_t cmd = SWITCH;

	viuoc_set(cmd);

	while(atomic_read(&shutdown)) {
		wait_for_completion(&send_data_lock);

		switch(sh_iuoc->received_data) {
			case SWITCH:
				cmd = SWITCH;
				break;
			case BLIND:
				cmd = BLIND;
				break;
			default:
				DBG("switch postion %d not supported\n", sh_iuoc->sw_pos);
				continue;
		}
		viuoc_set(cmd);
	}
	return 0;
}

void *app_thread_main(void *args) {
	tcb_t *iuoc_th;

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();
	printk("Welcome to IUOC ME\n");

	iuoc_th = kernel_thread(iuoc_send_cmd, "iuoc_send_command", NULL, THREAD_PRIO_DEFAULT);
	thread_join(iuoc_th);

	return 0;
}
