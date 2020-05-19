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

#include <soo/dev/vnetstream.h>

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
	while (1) {
		printk("%s: in loop within domain %d...\n", __func__, ME_domID());
#if defined(CONFIG_RTOS)
		//avz_sched_sleep_ms(300);
		msleep(300);
#else
		msleep(300);
#endif /* CONFIG_RTOS */

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

#if 0
static int netstream_task_fn(void *arg) {

	llprintk(" ********************** Netstream start *********************\n");

	while (1) {
		wait_for_completion(&compl);

		msleep(1400);
		llprintk("N\n");

	}

	return 0;
}
#endif
#if 0
static int audio_task_fn(void *arg) {

	lprintk("Audio start\n");

	while (1) {
		complete(&compl);
		//msleep(1000);

		lprintk("A");

	}

	return 0;
}
#endif

timer_t timer;

void timer_fn(void *dummy) {
	lprintk("### TIMER FIRED\n");
}

#if 1
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
		printk("(%d)",  ME_domID());
		printk("%c ", *((char *) localinfo_data));
	}

	return 0;
}
#endif

/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
int main_kernel(void *args) {
	agency_ctl_args_t agency_ctl_args;

	printk("SOO Mobile Entity booting ...\n");

	soo_guest_activity_init();

	callbacks_init();

	/* Initialize the Vbus subsystem */
	vbus_init();

	gnttab_init();

	vbstore_init_dev_populate();

	agency_ctl_args.cmd = AG_AGENCY_UID;
	if (agency_ctl(&agency_ctl_args) < 0)
		BUG();
	memcpy(&my_agencyUID, &agency_ctl_args.u.agencyUID_args.agencyUID, SOO_AGENCY_UID_SIZE);
	DBG("Agency UID: "); DBG_BUFFER(&my_agencyUID, SOO_AGENCY_UID_SIZE);

	/* Initializing debugging facilities */
	init_dbgevent();
	set_dbgevent_mode(DBGLIST_CONTINUOUS);
	init_dbglist();
	set_dbglist_mode(DBGLIST_CONTINUOUS);

	avz_shared_info->dom_desc.u.ME.spad.valid = true;

	printk("SO3  Mobile Entity -- main app v2.0 -- Copyright (c) 2016-2020 REDS Institute (HEIG-VD)\n\n");

	DBG("ME running as domain %d\n", ME_domID());

#if 0
	kernel_thread(thread1, "thread1", NULL, 0);
#endif

	/* Test phase 2: threaded activity */
#if 0
	init_completion(&compl);

	kernel_thread(netstream_task_fn, "netstream", NULL, 15);
	kernel_thread(audio_task_fn, "audio", NULL, 0);
#endif

	//init_timer(&timer, timer_fn, NULL);
#if 1
	*((char *) localinfo_data) = 'A';

	kernel_thread(alphabet_fn, "alphabet", NULL, 0);
#endif

#if defined(CONFIG_NETSTREAM_MESSAGING)
	{
		extern void messaging_init(void);
		messaging_init();
	}
#endif

	return 0;
}
