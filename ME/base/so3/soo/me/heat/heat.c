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
#include <soo/dev/vtemp.h>
#include <soo/dev/vvalve.h>


#include <device/irq.h>

/* Null agency UID to check if an agency UID is valid */
agencyUID_t null_agencyUID = {
	.id = { 0 }
};

/* My agency UID */
agencyUID_t my_agencyUID = {
	.id = { 0 }
};

/* Bool telling that at least 1 post-activate has been performed */
bool post_activate_done = false;

struct completion compl;
mutex_t lock1, lock2;

extern void *localinfo_data;


/*
 * Just an example using a thread.
 */
int thread1(void *args)
{

	vtemp_data_t temp_data;
	char buff[10];

	while (1) {

		if(vtemp_get_temp_data(&temp_data) > 0) {

			lprintk("ME SOO.heat : dev_id = %d, dev_type = %d, temp = %d\n", temp_data.dev_id, temp_data.dev_type, temp_data.temp);

			sprintf(buff, "%d-%d-%d\r\n", temp_data.dev_id, temp_data.dev_type, temp_data.temp);

			vvalve_generate_request(buff);
		}

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

#if 0 /* Stress test on evtchn and IRQs */
static int alphabet_fn(void *arg) {
	int res;
	unsigned int evtchn;
	struct evtchn_alloc_unbound alloc_unbound;

	printk("Alphabet roundtrip...\n");

#if 0
	set_timer(&timer, NOW() + SECONDS(10));
#endif

	/* Allocate an event channel associated to the ring */
	alloc_unbound.remote_dom = 0;
	alloc_unbound.dom = DOMID_SELF;

	hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);
	evtchn = alloc_unbound.evtchn;
	lprintk("## evtchn got from avz: %d\n", evtchn);

	res = bind_evtchn_to_irq_handler(evtchn, evt_interrupt, NULL, NULL);

	do_sync_dom(0, DC_PRE_SUSPEND);

	while (1) {

		/* printk("### heap size: %x\n", heap_size()); */
		msleep(500);

		/* Simply display the current letter which is incremented each time a ME comes back */
		lprintk("(%d)",  ME_domID());
		//printk("%c ", *((char *) localinfo_data));
		lprintk("X ");
	}

	return 0;
}

#endif


#if 0


/* Used to test a ME trip within a scalable network */

static int alphabet_fn(void *arg) {

	printk("Alphabet roundtrip...\n");

#if 0
	set_timer(&timer, NOW() + SECONDS(10));
#endif

	while (1) {

		/* printk("### heap size: %x\n", heap_size()); */
		msleep(500);

		/* Simply display the current letter which is incremented each time a ME comes back */
		lprintk("(%d)",  ME_domID());
		printk("%c ", *((char *) localinfo_data));

	}

	return 0;
}
#endif
#if 0

void heat_init(void) {

	temp_data = (vtemp_data_t *)localinfo_data;
	memset(temp_data, 0, sizeof(vtemp_data_t));

}
#endif

/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
int app_thread_main(void *args) {

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

#if 1
	kernel_thread(thread1, "thread1", NULL, 0);
	// heat_init();
#endif

	//init_timer(&timer, timer_fn, NULL);
	lprintk("SOO.heat Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");
#if 0


	*((char *) localinfo_data) = 'H';

	kernel_thread(alphabet_fn, "alphabet", NULL, 0);
#endif


	return 0;
}
