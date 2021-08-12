/*
 * Copyright (C) 2021 Thomas Rieder <thomas.rieder@heig-vd.ch>
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

#include <me/dogablind/dogablind.h>

#include <soo/dev/vdogablind.h>
#include <soo/dev/venoceansw.h>

#include <completion.h>


#include <device/irq.h>


/* Null agency UID to check if an agency UID is valid */
agencyUID_t null_agencyUID = {
	.id = { 0 }
};

/* My agency UID */
agencyUID_t my_agencyUID = {
	.id = { 0 }
};

/* ID of the Smart Object on which the ME is running. 0xff means that it has not been initialized yet. */
uint32_t my_id = 0xff;

/* Agency UID of the Smart Object on which the Entity has been injected */
agencyUID_t origin_agencyUID;

/* Name of the Smart Object on which the Entity has been injected */
char origin_soo_name[SOO_NAME_SIZE];

/* Agency UID of the Smart Object on which the Entity has migrated */
agencyUID_t target_agencyUID;

/* Name of the Smart Object on which the Entity has migrated */
char target_soo_name[SOO_NAME_SIZE];

/* Bool telling that the ME is in a Smart Object with the expected devcaps */
bool available_devcaps = false;

/* Bool telling that a remote application is connected */
bool remote_app_connected = false;

/* Detection of a ME that has migrated on another new Smart Object */
bool has_migrated = false;



/* Bool telling that at least 1 post-activate has been performed */
bool post_activate_done = false;

struct completion compl;
mutex_t lock1, lock2;

extern void *localinfo_data;
dogablind_data *g_dogablind_data;

/**
 * @brief Main thread sending command to the Lahoco blind via KNX
 **/
int soo_dogablind_command_blind(void *args)
{
	int sw_cmd, sw_id;

	while (1) {
		
		/* blocking until switch is pressed and retourned by BE*/
		get_sw_data(&sw_cmd, &sw_id);

		lprintk("ME SOO.dogablind receive %02hhx cmd from switch[0x%08X]\n", sw_cmd, sw_id);

		switch (sw_cmd)
		{
		case SWITCH_IS_RELEASED:
			vdogablind_send_blind_cmd(VDOGABLIND_STOP_CMD);
			g_dogablind_data->blind_cmd = VDOGABLIND_STOP_CMD;
			break;
		case SWITCH_IS_DOWN:
			vdogablind_send_blind_cmd(VDOGABLIND_DOWN_CMD);
			g_dogablind_data->blind_cmd = VDOGABLIND_DOWN_CMD;
			break;
		case SWITCH_IS_UP:
			vdogablind_send_blind_cmd(VDOGABLIND_UP_CMD);
			g_dogablind_data->blind_cmd = VDOGABLIND_UP_CMD;
			break;
		default:
			lprintk("ME SOO.dogablind : receive unkown sw cmd from FE/BE\n");
			break;
		}

		g_dogablind_data->sw_id = sw_id;

		/* migrate to find ME SOO.knxblind*/
		g_dogablind_data->need_propagate = true;

	}

	return 0;
}

void dumpPage(unsigned int phys_addr, unsigned int size) {
	int i, j;

	lprintk("%s: phys_addr: %lx\n\n", __func__,  phys_addr);

	for (i = 0; i < size; i += 16) {
		lprintk(" [%lx]: ", i);
		for (j = 0; j < 16; j++) {
			lprintk("%02x ", *((unsigned char *) __va(phys_addr)));
			phys_addr++;
		}
		lprintk("\n");
	}
}

timer_t timer;

void timer_fn(void *dummy) {
	lprintk("### TIMER FIRED\n");
}


irq_return_t evt_interrupt(int irq, void *dev_id) {

	lprintk("## got evt interrupt (irq %d)\n", irq);

	return IRQ_COMPLETED;
}


/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
int app_thread_main(void *args) {

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	lprintk("====== ME SOO.dogablind app_thread_main\n");

	kernel_thread(soo_dogablind_command_blind, "soo_dogablind_command_blind", NULL, 0);

	//init_timer(&timer, timer_fn, NULL);
	lprintk("SOO.dogablind Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");

	return 0;
}
