/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

/*
 * SOOlink network simulator
 */

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <soo/netsimul.h>

#include <soo/dcm/dcm.h>

#include <soo/soolink/discovery.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/datalink.h>

#include <soo/core/sysfs.h>
#include <soo/core/device_access.h>

sl_desc_t *sl_desc;

#define BUFFER_SIZE 16*1024*1024

static unsigned char buffer[BUFFER_SIZE];

static unsigned int count = 0;
struct mutex count_lock;

/* SOO instance handling */
struct list_head soo_environment;

soo_env_t *__current_soo(void) {
	soo_env_t *soo;
	soo_env_thread_t *soo_thread;

	list_for_each_entry(soo, &soo_environment, list)
	{
		list_for_each_entry(soo_thread, &soo->threads, list)
				if (soo_thread->pid == current->pid)
					return soo;
	}

	/* Should never happen */

	BUG();

	return NULL;
}

soo_env_t *get_soo_by_name(char *name) {
	soo_env_t *soo;

	list_for_each_entry(soo, &soo_environment, list)
	{
		if (!strcmp(soo->name, name))
			return soo;
	}

	return NULL;
}

void iterate_on_other_soo(soo_iterator_t fn, void *args) {
	soo_env_t *soo;
	bool cont = true;

	list_for_each_entry(soo, &soo_environment, list)
	{
		if (soo != current_soo)
			cont = fn(soo, args);

		if (!cont)
			break;
	}
}

void add_thread(soo_env_t *soo, unsigned int pid) {
	soo_env_thread_t *soo_env_thread;

	soo_env_thread = kzalloc(sizeof(soo_env_thread_t), GFP_KERNEL);
	BUG_ON(!soo_env_thread);

	soo_env_thread->pid = pid;

	list_add_tail(&soo_env_thread->list, &soo->threads);

}

static int soo_stream_task_rx_fn(void *args) {
	uint32_t size;
	void *data;
	int i;

	while (true){
		size = sl_recv(sl_desc, &data);

		for (i = 0; i < BUFFER_SIZE; i++)
			if (((unsigned char *) data)[i] != buffer[i]) {
				printk("## Data corruption : failure on byte %d\n", i);
				break;
			}

		if (i == BUFFER_SIZE) {
			count++;
			lprintk("## ******************** Got a buffer (count %d got %d bytes)\n", count, size);
		}

		/* Must release the allocated buffer */
		vfree(data);
	}

	return 0;
}

void stream_count_read(char *str) {
	sprintf(str, "%d", count);
}


/*
 * Testing RT task to send a stream to a specific smart object.
 * This is mainly used for debugging purposes and performance assessment.
 */
static int soo_stream_task_tx_fn(void *args) {
	int i;

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	sl_desc = sl_register(SL_REQ_DCM, SL_IF_WLAN, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	sl_desc = sl_register(SL_REQ_DCM, SL_IF_ETH, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_SIMULATION)
	sl_desc = sl_register(SL_REQ_DCM, SL_IF_SIMULATION, SL_MODE_UNIBROAD);
#else
#error !! You must specify a plugin interface in the kernel configuration !!
#endif

	for (i = 0; i < BUFFER_SIZE; i++)
		buffer[i] = i;

	soo_sysfs_register(stream_count, stream_count_read, NULL);

	while (true) {
#if 0
		if (discovery_neighbour_count() > 0) {
			lprintk("*** sending buffer ****\n");
			sl_send(sl_desc, buffer, BUFFER_SIZE, get_null_agencyUID(), 10);

			lprintk("*** sending COMPLETE ***\n");
			sl_send(sl_desc, NULL, 0, get_null_agencyUID(), 10);

			lprintk("*** End. ***\n");
		} else
#endif
			schedule();

	}

	return 0;
}

/*
 * Thread function dedicated to a smart object (SOO).
 */
int soo_env_fn(void *args) {
	struct task_struct *__ts;
	soo_env_t *soo_env;

	soo_env = kzalloc(sizeof(soo_env_t), GFP_KERNEL);
	BUG_ON(!soo_env);

	INIT_LIST_HEAD(&soo_env->threads);

	list_add_tail(&soo_env->list, &soo_environment);

	soo_env->id = count;
	mutex_lock(&count_lock);
	count++;
	mutex_unlock(&count_lock);

	strcpy(soo_env->name, (char *) args);

	/* Adding ourself (the current thread) to this environment. */
	add_thread(soo_env, current->pid);

	/* Generate a unique agencyUID. */
	get_random_bytes((void *) &soo_env->agencyUID, SOO_AGENCY_UID_SIZE);

	lprintk("[soo:core:device_access] On CPU %d, SOO %s has the Agency UID: ", smp_processor_id(), soo_env->name);
	lprintk_buffer((uint8_t *) soo_env->agencyUID, SOO_AGENCY_UID_SIZE);

	transceiver_init();

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
	plugin_simulation_init();
#elif
	memcpy((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, soo_env->agencyUID, SOO_AGENCY_UID_SIZE);
#endif

	/* Initializing the Discovery and Datalink functional blocks. */
	lprintk("[soo:core] Initializing SOOlink Discovery...\n");
	discovery_init(soo_env);

	lprintk("[soo:core] Initializing SOOlink Datalink...\n");
	datalink_init();

	/* Ready to initializing the DCM subsystem */
	//dcm_init();

	lprintk("[soo:core] Now, starting simulation threads...\n");

	/* Start activities - Simulation mode */
#if 0
	__ts = kthread_create(soo_stream_task_tx_fn, NULL, "soo_stream_task_tx");
	BUG_ON(!__ts);

	add_thread(soo_env, __ts->pid);
	wake_up_process(__ts);

	__ts = kthread_create(soo_stream_task_rx_fn, NULL, "soo_stream_task_rx");
	BUG_ON(!__ts);

	add_thread(soo_env, __ts->pid);
	wake_up_process(__ts);
#endif

	do_exit(0);

	return 0;
}


void soolink_netsimul_init(void) {

	lprintk("%s: starting SOO environment...\n", __func__);

	INIT_LIST_HEAD(&soo_environment);
	mutex_init(&count_lock);

	kthread_run(soo_env_fn, "SOO-1", "SOO-1");
	kthread_run(soo_env_fn, "SOO-2", "SOO-2");

}
