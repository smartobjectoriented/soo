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
 * SOO Ecosystem environment management
 *
 * Of course, a smart object must handle a set of state/general variables which
 * are very specific to a specific SOO instance.
 *
 * In the case of a simulated environment, it is possible to start several SOO instances
 * so that interactions between smart objects can be investigated.
 */

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <soo/sooenv.h>
#include <soo/hypervisor.h>
#include <soo/simulation.h>

#include <soo/dcm/dcm.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/transcoder.h>

#include <soo/core/sysfs.h>
#include <soo/core/device_access.h>

/*
 * The following structure is used to maintain a list
 * of callback functions which will be called once
 * all SOO subsystems will be fully initialized.
 */
typedef struct {

	struct list_head list;

	/* Callback function */
	sooenv_up_fn_t up_fn;

	/* Pointer which can be retrieved in the callback function */
	void *args;

} sooenv_up_t;

LIST_HEAD(sooenv_up_list);

/* SOO instance handling */
struct list_head soo_environment;

static int count = 0;
static struct mutex env_lock;

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION

#define	SOO_NR_MAX 	10

	soo_env_t *soo1, *soo2, *soo3, *soo4, *soo5, *soo6;
#endif

/**
 * Get a reference to the current SOO environment.
 * @return the address soo_env
 */
soo_env_t *__current_soo(void) {
	soo_env_t *soo;
	soo_env_thread_t *soo_thread;

	BUG_ON(list_empty(&soo_environment));

	if (__domcall_in_progress)
		return list_first_entry(&soo_environment, soo_env_t, list);

	list_for_each_entry(soo, &soo_environment, list)
	{
		list_for_each_entry(soo_thread, &soo->threads, list)
			if (soo_thread->pid == current->pid)
				return soo;
	}

	/* If we did not find a SOO env in the list of threads, it means
	 * we are not in a context of simulation. We take the first one.
	 */

	return list_first_entry(&soo_environment, soo_env_t, list);
}

/**
 * Get a reference to the SOO environment by its name.
 * @param name (SOO name)
 * @return the address of the soo_env
 */
soo_env_t *get_soo_by_name(char *name) {
	soo_env_t *soo;

	list_for_each_entry(soo, &soo_environment, list)
	{
		if (!strcmp(soo->name, name))
			return soo;
	}

	return NULL;
}

void dump_soo(void) {
	soo_env_t *soo;

	lprintk("[soo:soo_env] List of registered SOO:\n\n");

	list_for_each_entry(soo, &soo_environment, list)
		lprintk("[soo:soo_env] %s\n", soo->name);
}

void add_thread(soo_env_t *soo, unsigned int pid) {
	soo_env_thread_t *soo_env_thread;

	soo_env_thread = kzalloc(sizeof(soo_env_thread_t), GFP_KERNEL);
	BUG_ON(!soo_env_thread);

	soo_env_thread->pid = pid;

	list_add_tail(&soo_env_thread->list, &soo->threads);

}

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION

static int soo_task_rx_fn(void *args) {
	uint32_t size;
	char *data;
	int i;

	int soo_count_table[SOO_NR_MAX] = { 0 };

	while (true) {

		size = sl_recv(current_soo_simul->sl_desc, (void *) &data);

		for (i = 1; i < BUFFER_SIZE; i++)
			if (((unsigned char *) data)[i] != current_soo_simul->buffer[i]) {
				lprintk("## Data corruption : failure on byte %d\n", i);
				break;
			}

		soo_count_table[(int) data[0]]++;

		if (i == BUFFER_SIZE) {
			current_soo_simul->recv_count++;
			lprintk("## (%s) ******************** Got a buffer (count %d got %d bytes)\n", current_soo->name, current_soo_simul->recv_count, size);
			lprintk("## stats: ");
			for (i = 1; i < SOO_NR_MAX; i++)
				lprintk(" (SOO-%d): %d ", i, soo_count_table[i]);

			lprintk("\n");
		}

		/* Must release th e allocated buffer */
		vfree(data);
	}

	return 0;
}

void buffer_count_read(char *str) {
	sprintf(str, "%d", current_soo_simul->recv_count);
}

/*
 * Testing RT task to send data to a specific smart object.
 * This is mainly used for debugging purposes and performance assessment.
 */
static int soo_task_tx_fn(void *args) {
	int i;

	current_soo_simul->sl_desc = sl_register(SL_REQ_DCM, SL_IF_SIM, SL_MODE_UNIBROAD);

	for (i = 0; i < BUFFER_SIZE; i++)
		current_soo_simul->buffer[i] = i;

	soo_sysfs_register(buffer_count, buffer_count_read, NULL);

	while (true) {
		if (discovery_neighbour_count() > 0) {
			lprintk("*** (%s) sending buffer ****\n", current_soo->name);

			/* Encode the SOO number */
			current_soo_simul->buffer[0] = current_soo->id;

			sl_send(current_soo_simul->sl_desc, current_soo_simul->buffer, BUFFER_SIZE, get_null_agencyUID(), 10);

			lprintk("*** (%s) sending COMPLETE ***\n", current_soo->name);

			sl_send(current_soo_simul->sl_desc, NULL, 0, get_null_agencyUID(), 10);
			lprintk("*** (%s) End. ***\n", current_soo->name);

			//msleep(1000);

		} else
			schedule();
	}

	return 0;
}
/**
 *
 * Specific behaviour of SOO3 emitter side.
 */
static int soo3_task_tx_fn(void *args) {
	int i;
	bool here = true;

	current_soo_simul->sl_desc = sl_register(SL_REQ_DCM, SL_IF_SIM, SL_MODE_UNIBROAD);

	for (i = 0; i < BUFFER_SIZE; i++)
		current_soo_simul->buffer[i] = i;

	soo_sysfs_register(buffer_count, buffer_count_read, NULL);

	i = 0;
	while (true) {
		if (here && discovery_neighbour_count() > 0) {
			lprintk("*** (%s) sending buffer ****\n", current_soo->name);

			/* Encode the SOO number */
			current_soo_simul->buffer[0] = current_soo->id;

			sl_send(current_soo_simul->sl_desc, current_soo_simul->buffer, BUFFER_SIZE, get_null_agencyUID(), 10);

			lprintk("*** (%s) sending COMPLETE ***\n", current_soo->name);

			sl_send(current_soo_simul->sl_desc, NULL, 0, get_null_agencyUID(), 10);
			lprintk("*** (%s) End. ***\n", current_soo->name);

#if 0
			i++;
			if (i == 5) {
				node_unlink(soo3, soo1);
				node_unlink(soo3, soo2);
				here = false;
			}
#endif
		} else
			schedule();
	}

	return 0;
}
#endif /* CONFIG_SOOLINK_PLUGIN_SIMULATION */

/*
 * Thread function dedicated to a smart object (SOO).
 */
int soo_env_fn(void *args) {
	soo_env_t *soo_env;
	sooenv_up_t *sooenv_up, *tmp;

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
	int i;
	struct task_struct *__ts;
#endif

	soo_env = kzalloc(sizeof(soo_env_t), GFP_KERNEL);
	BUG_ON(!soo_env);

	INIT_LIST_HEAD(&soo_env->threads);

	mutex_lock(&env_lock);

	count++;
	list_add_tail(&soo_env->list, &soo_environment);

	soo_env->id = count;

	mutex_unlock(&env_lock);

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
	strcpy(soo_env->name, (char *) args);
#else
	devaccess_get_soo_name(soo_env->name);
#endif

	/* Adding ourself (the current thread) to this environment. */
	add_thread(soo_env, current->pid);

	/* Generate a unique agencyUID. */
#ifndef CONFIG_SOOLINK_PLUGIN_SIMULATION

	get_random_bytes((void *) &soo_env->agencyUID, SOO_AGENCY_UID_SIZE);

#else

	for (i = 0; i < SOO_AGENCY_UID_SIZE; i++)
		soo_env->agencyUID.id[i] = 0x00;

	soo_env->agencyUID.id[3] = 0x99;

	soo_env->agencyUID.id[4] = count;

#endif /* CONFIG_SOOLINK_PLUGIN_SIMULATION */

	soo_log("[soo:core:device_access] On CPU %d, SOO %s has the Agency UID: ", smp_processor_id(), soo_env->name);
	soo_log_printlnUID(&current_soo->agencyUID);

	/* Initializing SOOlink subsystem */
	soolink_init();

	transceiver_init();

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
	plugin_simulation_init();
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET
	memcpy((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, &soo_env->agencyUID, SOO_AGENCY_UID_SIZE);
	plugin_ethernet_init();
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN
	memcpy((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, &soo_env->agencyUID, SOO_AGENCY_UID_SIZE);
	plugin_wlan_init();
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH
	memcpy((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, &soo_env->agencyUID, SOO_AGENCY_UID_SIZE);
	plugin_bt_init();
#endif

	/* Initializing the Discovery and Datalink functional blocks. */
	lprintk("[soo:core] Initializing SOOlink Discovery...\n");
	discovery_init();

	lprintk("[soo:core] Initializing SOOlink Datalink...\n");
	datalink_init();

	/* Transcoder initialization */
	transcoder_init();

	/* In simulation mode, we do not manage several instances of DCM yet. */
#ifndef CONFIG_SOOLINK_PLUGIN_SIMULATION
	/* Ready to initializing the DCM subsystem */
	dcm_init();
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION

	lprintk("[soo:core] Now, starting SOO simulation threads...\n");

	/* Start activities - Simulation mode */

	/* Prepare the environment */
	soo_env->soo_simul = kzalloc(sizeof(struct soo_simul_env), GFP_KERNEL);
	BUG_ON(!soo_env->soo_simul);

	INIT_LIST_HEAD(&soo_env->soo_simul->topo_links);

	current_soo_simul->recv_count = 0;

	if (!strcmp(soo_env->name, "SOO-3")) {
		__ts = kthread_create(soo3_task_tx_fn, NULL, "soo3_task_tx");
		BUG_ON(!__ts);

	} else {
		__ts = kthread_create(soo_task_tx_fn, NULL, "soo_task_tx");
		BUG_ON(!__ts);
	}

	add_thread(soo_env, __ts->pid);
	wake_up_process(__ts);

	__ts = kthread_create(soo_task_rx_fn, NULL, "soo_task_rx");
	BUG_ON(!__ts);

	add_thread(soo_env, __ts->pid);
	wake_up_process(__ts);
#endif

	soo_env->ready = true;

	/* Performed all deferred init functions which were
	 * waiting for all SOO subsystems get fully initialized.
	 */

	if (soo_env->id == 1)
		list_for_each_entry_safe(sooenv_up, tmp, &sooenv_up_list, list) {

			/* Execute the callback function */
			sooenv_up->up_fn(soo_env, sooenv_up->args);

			list_del(&sooenv_up->list);
			kfree(sooenv_up);

			break;
		}

	do_exit(0);

	return 0;
}

/**
 * Registering a new callback function which will be called
 * at the end of the SOO environment initialization process.
 *
 * @param sooenv_up_fn	callback function
 * @param args		arguments to be passed to the callback function
 */
void register_sooenv_up(sooenv_up_fn_t sooenv_up_fn, void *args) {
	sooenv_up_t *sooenv_up;

	sooenv_up = kzalloc(sizeof(sooenv_up_t), GFP_KERNEL);
	BUG_ON(!sooenv_up);

	sooenv_up->up_fn = sooenv_up_fn;
	sooenv_up->args = args;

	list_add_tail(&sooenv_up->list, &sooenv_up_list);
};

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION

void sooenv_init_topology(soo_env_t *sooenv, void *args) {

	if (!strcmp(sooenv->name, "SOO-3")) {
		soo1 = get_soo_by_name("SOO-1");
		soo2 = get_soo_by_name("SOO-2");
		soo3 = get_soo_by_name("SOO-3");
		soo4 = get_soo_by_name("SOO-4");
		soo5 = get_soo_by_name("SOO-5");
		soo6 = get_soo_by_name("SOO-6");
		
		BUG_ON(!soo1 || !soo2 || !soo3 || !soo4 || !soo5 || !soo6);

		/* Define the SOO topology */

		node_link(soo1, soo2); node_link(soo2, soo1);
		node_link(soo1, soo3); node_link(soo3, soo1);
		node_link(soo2, soo3); node_link(soo3, soo2);
		node_link(soo3, soo4); node_link(soo4, soo3);
		node_link(soo4, soo5); node_link(soo5, soo4);
		node_link(soo5, soo6); node_link(soo6, soo5);
	}
}

#endif

/**
 * sooenv_init() is called from a thread at the end
 * of the boot process, which is created in init/main.c
 * before the rootfs mounting.
 */
void sooenv_init(void) {

	lprintk("%s: starting SOO environment...\n", __func__);

	INIT_LIST_HEAD(&soo_environment);
	mutex_init(&env_lock);

	kthread_run(soo_env_fn, "SOO-1", "SOO-1");

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION

	kthread_run(soo_env_fn, "SOO-2", "SOO-2");
	kthread_run(soo_env_fn, "SOO-3", "SOO-3");
	kthread_run(soo_env_fn, "SOO-4", "SOO-4");
	kthread_run(soo_env_fn, "SOO-5", "SOO-5");
	kthread_run(soo_env_fn, "SOO-6", "SOO-6");

	register_sooenv_up(sooenv_init_topology, NULL);

#endif

}
