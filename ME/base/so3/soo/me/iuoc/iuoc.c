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
 * Description: This file is the implementation of the IUOC ME. This code is 
 * responsible of managing the data incoming from any ME and from the IUOC server
 * that are allowed to communicate with the IUOC.
 */

#if 1
#define DEBUG
#endif

#include <thread.h>
#include <string.h>

#include <soo/xmlui.h>

#include <soo/dev/viuoc.h>
#include <me/iuoc.h>
#include <soo/dev/viuoc.h>
#include <delay.h>


iuoc_data_t data_debug;
int debug_count = 0; 
field_data_t field_debug;

void *iuoc_send_cmd(void *args) {
	while (1) {
	    udelay(3000000);

		data_debug.me_type = IUOC_ME_BLIND;
		data_debug.timestamp = 20 * debug_count++ ;
		strcpy(field_debug.name, "action");  
		strcpy(field_debug.type, "int");  
		field_debug.value = 3;
		data_debug.data_array[0] = field_debug;
		data_debug.data_array_size = 1;
		viuoc_set(data_debug);
	}

	return 0;
}

/**
 * @brief Thread to acquire iuoc events
 * 
 * @param args not used for now
 */
void *iuoc_wait_data_th(void *args) {
	iuoc_data_t iuoc_data;
	int ret;

	printk("[IUOC front] ME thread receiver set up !\n");

	while (1) {
		printk("[IUOC front] ME thread waiting for new data\n");
		ret = get_iuoc_me_data(&iuoc_data);
		printk("[IUOC front] ME thread got a new data\n");
		if (ret) {
			continue;
		}

		printk ("Data : ME_type=%d, timestamp=%d, array_size=%d\n", 
				iuoc_data.me_type, iuoc_data.timestamp, iuoc_data.data_array_size);
	}

}

void *app_thread_main(void *args) {
	tcb_t *iuoc_th;
	tcb_t *iuoc_recv_th;

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();
	printk("Welcome to IUOC ME\n");

	iuoc_th = kernel_thread(iuoc_send_cmd, "iuoc_send_command", NULL, THREAD_PRIO_DEFAULT);

	iuoc_recv_th = kernel_thread(iuoc_wait_data_th, "iuoc_wait_data", NULL, THREAD_PRIO_DEFAULT);

	thread_join(iuoc_th);
	thread_join(iuoc_recv_th);

	return 0;
}
