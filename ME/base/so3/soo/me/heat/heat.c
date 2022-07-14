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

#if 1
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
#include <soo/dev/vvalve.h>
#include <me/heat.h>

#include <completion.h>
#include <device/irq.h>

#define TMP_WANTED	25


/* ID of the Smart Object on which the ME is running. 0xff means that it has not been initialized yet. */
uint32_t my_id = 0xff;

/* Name of the Smart Object on which the Entity has been injected */
char origin_soo_name[SOO_NAME_SIZE];

/* Bool telling that the ME is in a Smart Object with the expected devcaps */
bool available_devcaps = false;

/* Bool telling that a remote application is connected */
bool remote_app_connected = false;

/* Detection of a ME that has migrated on another new Smart Object */
bool has_migrated = false;



/* Bool telling that at least 1 post-activate has been performed */
bool post_activate_done = false;

extern void *localinfo_data;

/**
 * @brief Main thread sending command to the valve when SOO.indoor cooperate
 **/
void *soo_heat_command_valve(void *args)
{

	int valve_id;

	while (atomic_read(&shutdown)) { 

		DBG("ME SOO.heat is waiting\n");
		wait_for_completion(&send_data_lock);
		DBG("ME SOO.heat going to send valve cmd !!!\n");

		/* Get ID of the valve connected on the current Smart Object*/
		valve_id = vvalve_get_id();
		// valve_id = 0;
		DBG(MEHEAT_PREFIX "Valve ID : %d\n", valve_id);
		DBG(MEHEAT_PREFIX "Heat ID : %d\n", sh_heat->heat.id);
		/* Compare the temperature Sensor ID from SOO.indoor with the valve ID */
		if(sh_heat->heat.id == valve_id) {
			DBG(MEHEAT_PREFIX "Check temperatur for the valve\n");
			if(sh_heat->heat.temperature > TMP_WANTED) {
				DBG(MEHEAT_PREFIX "Valve is going to close\n");
				vvalve_send_cmd(VALVE_CMD_CLOSE);

			} else {
				DBG(MEHEAT_PREFIX "Valve is going to open\n");
				vvalve_send_cmd(VALVE_CMD_OPEN);
			}
		} else {
			DBG(MEHEAT_PREFIX "Not the same ID\n");
			/*TODO: ME SOO.indoor should migrate on another Smart Object */
		}
	}

	return NULL;
}

void dumpPage(unsigned int phys_addr, unsigned int size) {
	int i, j;

	DBG("%s: phys_addr: %lx\n\n", __func__,  phys_addr);

	for (i = 0; i < size; i += 16) {
		DBG(" [%lx]: ", i);
		for (j = 0; j < 16; j++) {
			DBG("%02x ", *((unsigned char *) __va(phys_addr)));
			phys_addr++;
		}
		DBG("\n");
	}
}

timer_t timer;

void timer_fn(void *dummy) {
	DBG("### TIMER FIRED\n");
}


irq_return_t evt_interrupt(int irq, void *dev_id) {

	DBG("## got evt interrupt (irq %d)\n", irq);

	return IRQ_COMPLETED;
}


/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
void *app_thread_main(void *args) {
	tcb_t *heat_th;
	heat_t *heat;

	heat = (heat_t *)malloc(sizeof(heat_t));
	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	DBG(MEWEATHERSTATION_PREFIX "Welcome\n");
	heat_th = kernel_thread(soo_heat_command_valve, "soo_heat_command_valve", heat, THREAD_PRIO_DEFAULT);

	if(!heat_th){
		DBG(MEHEAT_PREFIX "Failed to start heat thread\n");
		kernel_panic();
	}
	//init_timer(&timer, timer_fn, NULL);
	DBG("SOO.heat Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");

	return NULL;
}
