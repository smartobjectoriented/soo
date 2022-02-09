/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/irqreturn.h>

#include <linux/mmc/core.h>

#include <xenomai/rtdm/driver.h>

#include <opencn/ctypes/strings.h>

#include <soo/evtchn.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/logbool.h>

#include <soo/debug/bandwidth.h>
#include <soo/debug/time.h>

#include <soo/dev/vuart.h>

/*
 * This code is mainly for debugging purposes.
 *
 * It is the only code which may not be fully compliant with the coding convention.
 * But try to keep it as clean as possible anyway.
 */

#if 1 /* Testing code */
	rtdm_task_t rt_task1;
	rtdm_task_t rt_task2;
	rtdm_task_t rt_task3;
	rtdm_task_t rt_task4;
	rtdm_task_t rt_isr;
	rtdm_irq_t irq_handle;

	rtdm_task_t rt_task_wifi;
	char enable_dc_event_gen = 0;
	rtdm_task_t rt_task_dcevt;

	rtdm_event_t rt_event;

	rtdm_task_t second_task;

#endif

rtdm_timer_t rt_timer;

#if 0 /* Emulated PIRQ debugging */
rtdm_irq_t irq_handle;
static rtdm_event_t rt_isr_event;
int irq46_ready = false;
rtdm_mutex_t rt_mutex;

static void rtdm_isr_thread(void *arg) {

	lprintk("### Processing ISR threadé...\n");

	while (1) {
		//lprintk("<");
		rtdm_event_wait(&rt_isr_event);
		//lprintk(">");
	}
}

#endif
#if 0
extern void soolink_cycle_fct(void);
extern void rt_task_wifi_fct(void *args);

static void second_task_fn(void *args) {
	s64 t1, t2;

	while (1) {
		rtdm_task_wait_period(NULL);

		//lprintk("@");
		//ll_time_collect_period_show("", 0, "");

		t1 = ll_time_get();
		lprintk("0123456789");
		t2 = ll_time_get();
		lprintk_int64(t2 - t1);
	}
}
#endif
#if 0
static void my_rt_task_sub2(void *args) {
	int  i = 0;

	while (1) {
		lprintk("## *************** : %d\n", i);
		if (i == 5)
			return ;

		i++;

		rtdm_task_wait_period(NULL);

	}

}
#endif

#if 1
static void my_rt_task1_fn(void *args) {

	int i = 0;
	int toggle = 0;

	lprintk("Low frequency thread...\n");

#if 1
	while (1) {

		//rtdm_event_wait(&rt_event);
		//lprintk("## Got %d\n", i++);

		printk("## thread task1, idx: %d\n", i++);
#if 0
		if (!toggle) {

			rtdm_mutex_lock(&rt_mutex);
			toggle = 1;
		} else  {
			rtdm_mutex_unlock(&rt_mutex);
			toggle = 0;
		}
#endif

		rtdm_task_wait_period(NULL);

	}
#endif

}

static void rt_task2_fn(void *args) {

	while (1) {

		lprintk("## thread task 2\n");

		rtdm_task_wait_period(NULL);

	}
#if 0
	rtdm_event_signal(&rt_event);
	rtdm_event_signal(&rt_event);
#endif

}

static void rt_task3_fn(void *args) {

	int i = 0;
	int toggle = 0;

	lprintk("Low frequency thread...\n");


	while (1) {

		lprintk("## T3 i: %d\n", i++);
		//lprintk("### before wait...\n");
		//rtdm_event_wait(&rt_event);
		//lprintk("### after wait...\n");
		if (i == 5)
			return ;
		rtdm_task_wait_period(NULL);
	}
}

#endif

#if 0
static void my_rt_task(void *args) {
	int toggle = 0;

	lprintk("High frequency thread...\n");

	while (1) {
		lprintk("X");
		if (is_rtdm_wifi_enabled()) {
					rtdm_do_sync_agency(DC_INT_BT_RT);
				}

		if (!toggle) {

					rtdm_mutex_lock(&rt_mutex);
					toggle = 1;
				} else  {
					rtdm_mutex_unlock(&rt_mutex);
					toggle = 0;
				}

		rtdm_task_wait_period(NULL);
	}

}
#endif

#if 1
int rt_dummy_interrupt(rtdm_irq_t *dummy) {

	lprintk("### Got the interrupt on CPU: %d\n", smp_processor_id());

	return RTDM_IRQ_HANDLED;
}

static void rt_task_2_fn(void *args) {

	while (true) {
#if 0
		rtdm_task_sleep(MICROSECS(48));
		lprintk("X");

		lprintk("### before wait...\n");
		rtdm_event_wait(&rt_event);
		lprintk("### after wait...\n");
		if (rtdm_task_should_stop())
			return ;
		lprintk("## go for loop\n");
#endif
		lprintk("## In the periodic task of 1 s.\n");

		rtdm_task_wait_period(NULL);
	}

}
#endif

#if 0
static void rt_task_1_fct(void *args) {

	int i = 0;

/* Testing vbus */
#if 0
	{
		int res;

		res =  rtdm_bind_interdomain_evtchn_to_irqhandler(&irq_handle, 0, 15, rt_dummy_interrupt, 0, "dummy_irq", NULL);

		lprintk("### notifying with irq: %d\n", res);
		notify_remote_via_irq(res);
	}
#endif
#if 0
	rtdm_task_sleep(MILLISECS(1000));
	lprintk("### before destroy\n");
	rtdm_task_destroy(&rt_task2);
	lprintk("### after destroy\n");
#endif

	//rtdm_irq_request(&irq_handle, 93, NULL, 0, "rtdm_sunxi_mmc", NULL);
	while (1) {

		//rtdm_event_wait(&rt_event);

		lprintk("## i: %d\n", i);

		i++;

		rtdm_task_wait_period(NULL);

	}
}
#endif


void rt_timer_fn(rtdm_timer_t *rt_timer) {

	lprintk("### timer fired\n");

}

void first_dbgvar_action(void) {
	/* Nothing to do */
}

irqreturn_t evt_interrupt(int irq, void *dev_id)
{

	lprintk("## Got irq %d\n", irq);

	return IRQ_HANDLED;
}



void async_event_test(dc_event_t dc_event) {
	static int count = 0;
	int irq0;

	/* Send a PING */
	lprintk("## GOT the dc event to start stress test...\n");

	tell_dc_stable(DC_PRE_SUSPEND);

	irq0 = bind_interdomain_evtchn_to_virqhandler(2, 6, evt_interrupt, NULL, 0, "evt_6", NULL);

	while (true) {

		notify_remote_via_virq(irq0);

		do_sync_dom(2, DC_SUSPEND);
		do_sync_dom(2, DC_RESUME);
		do_sync_dom(2, DC_SUSPEND);
		do_sync_dom(2, DC_RESUME);
		do_sync_dom(2, DC_SUSPEND);
		do_sync_dom(2, DC_RESUME);


		msleep(600);
	}

}

int rtapp_main(void *args) {
	unsigned int *pgdir;
	volatile double tt;

	lprintk("RT Agency ready\n");

	pgdir = (unsigned int *) swapper_pg_dir;
#if 0
	tt = 3.25;

	tt = tt / 3.0;
	opencn_printf("## val: %12.7g\n", tt);
#endif

#if 0
	register_dc_event_callback(DC_PRE_SUSPEND, async_event_test);
#endif

#if 0

	ll_bandwidth_init();

	__ht_set = (ht_set_t) avz_start_info->logbool_ht_set_addr;
	{
	int ret;
	rtdm_event_init(&rt_isr_event, 0);
	ret = rtdm_irq_request(&irq_handle, 46, rt_isr_46, IRQF_NO_SUSPEND, "rt_isr_46", NULL);
	if (ret)
		BUG();

	irq46_ready = true;
	}
#endif


	//rtdm_mutex_init(&rt_mutex);

	//rtdm_task_init(&rt_isr, "rt_isr_t", rtdm_isr_thread, NULL, 90, 0);
	//rtdm_event_init(&rt_event, 0);

	rtdm_task_init(&rt_task1, "rt_task_1", my_rt_task1_fn, NULL, 50, 1000000000);
	//rtdm_task_init(&rt_task2, "rt_task_2", rt_task2_fn, NULL, 50, 300000000);

	//rtdm_task_init(&rt_task3, "rt_task_3", my_rt_task_sub, NULL, 50, 1000000000);


#if 0
	rtdm_event_init(&rt_event, 0);
	//rtdm_timer_init(&rt_timer, rt_timer_fn);

	//rtdm_task_init(&rt_task1, "rt_task_discovery", rt_task_discovery_fn, NULL, 50, SECONDS(1));

	rtdm_task_init(&rt_task2, "rt_task_2", rt_task2_fn, NULL, 50, SECONDS(1));
	rtdm_task_init(&rt_task3, "rt_task_3", rt_task3_fn, NULL, 50, MILLISECS(500));

	//rtdm_task_init(&rt_task4, "rt_task_4", rt_task_1_fct, NULL, 50, MILLISECS(50));
	//rtdm_task_init(&rt_task2, "rt_task_2", my_rt_task, NULL, 50, 2000000000);

	//rtdm_task_init(&rt_task_wifi, "rt_task_wifi", rt_task_wifi_fct, NULL, 50, 5000000);
#endif
#if 0
	kthread_run(nort_task_dcevt_fct, NULL, "nort_dcevt");
	rtdm_task_init(&rt_task_dcevt, "rt_dcevt", rt_task_dcevt_fct, NULL, 50, 500000);
#endif

	/* We can leave this thread die. Our system is living anyway... */
	do_exit(0);

	return 0;

}
