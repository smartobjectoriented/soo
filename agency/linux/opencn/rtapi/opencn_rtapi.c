/** RTAPI is a library providing a uniform API for several real time
 operating systems.
 This version is devoted to the opencn project.
 */
/********************************************************************
 * Description:  opencn_rtapi.c
 *               Realtime RTAPI implementation for the opencn-amp platform.
 *
 * Author: John Kasunich, Paul Corner
 * Authors: Jean-Pierre Miceli, Kevin Joly, Daniel Rossier
 * License: GPL Version 2
 *
 * Copyright (c) 2004 All rights reserved.
 * Copyright (c) 2019 REDS Institute, HEIG-VD
 *
 * Last change:
 ********************************************************************/


/** Copyright (C) 2003 John Kasunich
 <jmkasunich AT users DOT sourceforge DOT net>
 Copyright (C) 2003 Paul Corner
 <paul_c AT users DOT sourceforge DOT net>
 This library is based on version 1.0, which was released into
 the public domain by its author, Fred Proctor.  Thanks Fred!
 */

/* This library is free software; you can redistribute it and/or
 modify it under the terms of version 2 of the GNU General Public
 License as published by the Free Software Foundation.
 This library is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 harming persons must have provisions for completely removing power
 from all motors, etc, before persons enter any danger area.  All
 machinery must be designed to comply with local and national safety
 codes, and the authors of this software can not, and do not, take
 any responsibility for such compliance.

 This code was written as part of the EMC HAL project.  For more
 information, go to www.linuxcnc.org.
 */

#include <stdarg.h>		/* va_* */
#include <linux/kernel.h>
#include <linux/slab.h>		/* replaces malloc.h in recent kernels */
#include <linux/ctype.h>	/* isdigit */

#include <xenomai/rtdm/driver.h>

#include <linux/sched.h>
#include <asm/uaccess.h>	/* copy_from_user() */

/* get inb(), outb(), ioperm() */
#include <asm/io.h>

#include <linux/cpumask.h>	/* NR_CPUS, cpu_online() */

#include "vsnprintf.h"

#include <xenomai/rtdm/driver.h>

#include "rtapi.h"		/* public RTAPI decls */
#include <rtapi_mutex.h>
#include "rtapi_common.h"	/* shared realtime/nonrealtime stuff */

#include <opencn/logfile.h>

static rtdm_task_t *ostask_array[RTAPI_MAX_TASKS + 1];
static void *shmem_addr_array[RTAPI_MAX_SHMEMS + 1];
static rtdm_sem_t ossem_array[RTAPI_MAX_SEMS + 1];

#define DEFAULT_MAX_DELAY 10000
static long int max_delay = DEFAULT_MAX_DELAY;

bool *p_rtapi_log_enabled = NULL;

rtapi_log_sring_t *rtapi_log_sring = NULL;
static rtapi_log_front_ring_t rtapi_log_ring;

/*
 * In OpenCN, we do not need a periodic timer as a reference clock since the RT CPU already has
 * an independent clocksource as well as the local APIC timer as clockevent.
 */

/* comp parameters */
int msg_level = RTAPI_MSG_ALL;


/* the following are internal functions that do the real work associated
 with deleting tasks, etc.  They do not check the mutex that protects
 the internal data structures.  When someone calls an rtapi_xxx_delete()
 function, the rtapi funct gets the mutex before calling one of these
 internal functions.  When internal code that already has the mutex
 needs to delete something, it calls these functions directly.
 */
static int comp_delete(int comp_id);
static int task_delete(int task_id);
static int shmem_delete(int shmem_id, int comp_id);
static int sem_delete(int sem_id, int comp_id);


/***********************************************************************
 *                   INIT AND SHUTDOWN FUNCTIONS                        *
 ************************************************************************/

int rtapi_init(void) {
	int n;

	/* say hello */
	rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Init\n");
	/* get master shared memory block from OS and save its address */

	rtapi_data = rtdm_malloc(sizeof(rtapi_data_t));
	if (rtapi_data == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"RTAPI: ERROR: could not open shared memory\n");
		return -ENOMEM;
	}
	/* perform a global init if needed */
	init_rtapi_data(rtapi_data);

	/* check revision code */
	if (rtapi_data->rev_code != rev_code) {

		/* mismatch - release master shared memory block */
		rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: version mismatch %d vs %d\n", rtapi_data->rev_code, rev_code);

		rtdm_free(rtapi_data);

		return -EINVAL;
	}
	/* set up local pointers to global data */

	comp_array = rtapi_data->comp_array;
	task_array = rtapi_data->task_array;
	shmem_array = rtapi_data->shmem_array;
	sem_array = rtapi_data->sem_array;
	fifo_array = rtapi_data->fifo_array;
	irq_array = rtapi_data->irq_array;

	/* perform local init */
	for (n = 0; n <= RTAPI_MAX_TASKS; n++)
		ostask_array[n] = NULL;

	for (n = 0; n <= RTAPI_MAX_SHMEMS; n++)
		shmem_addr_array[n] = NULL;

	rtapi_data->timer_running = 0;
	rtapi_data->timer_period = 0;
	max_delay = DEFAULT_MAX_DELAY;

	rtapi_data->rt_cpu = AGENCY_RT_CPU;

#ifdef RTAPI_USE_PROCFS
	/* set up /proc/rtapi */
	if (proc_init() != 0) {
		rtapi_print_msg(RTAPI_MSG_WARN,
				"RTAPI: WARNING: Could not activate /proc entries\n");
		proc_clean();
	}
#endif

	/* Allocate the shared ring used to forward log messages. */
	rtapi_log_sring = (rtapi_log_sring_t *) kmalloc(LOG_RING_SIZE, GFP_ATOMIC);
	if (!rtapi_log_sring) {
		lprintk("%s - line %d: Allocating rtapi log shared ring failed.\n", __func__, __LINE__);
		BUG();
	}
	FRONT_RING_INIT(&rtapi_log_ring, rtapi_log_sring, LOG_RING_SIZE);

	/* done */
	rtapi_print_msg(RTAPI_MSG_INFO, "On CPU %d, RTAPI: Init complete\n", smp_processor_id());
	return 0;
}

/* This cleanup code attempts to fix any messes left by component
 that fail to load properly, or fail to clean up after themselves */

void rtapi_cleanup(void) {
	int n;

	if (rtapi_data == NULL) {
		/* never got inited, nothing to do */
		return;
	}

	/* grab the mutex */
	rtapi_mutex_get(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exiting\n");

	/* clean up leftover component (start at 1, we don't use ID 0 */
	for (n = 1; n <= RTAPI_MAX_COMP; n++) {
		if (comp_array[n].state == REALTIME) {
			rtapi_print_msg(RTAPI_MSG_WARN,
					"RTAPI: WARNING: comp '%s' (ID: %02d) did not call rtapi_exit()\n",
					comp_array[n].name, n);
			comp_delete(n);
		}
	}

	for (n = 1; n <= RTAPI_MAX_FIFOS; n++) {
		if (fifo_array[n].state != UNUSED) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"RTAPI: ERROR: FIFO %02d not deleted\n",
					n);
		}
	}
	for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
		while (sem_array[n].users > 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"RTAPI: ERROR: semaphore %02d not deleted\n",
					n);
		}
	}
	for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
		if (shmem_array[n].rtusers > 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"RTAPI: ERROR: shared memory block %02d not deleted\n",
					n);
		}
	}
	for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
		if (task_array[n].state != EMPTY) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"RTAPI: ERROR: task %02d not deleted\n",
					n);
			/* probably un-recoverable, but try anyway */

			task_delete(n);
		}
	}
	if (rtapi_data->timer_running != 0) {

		rtapi_data->timer_period = 0;
		rtapi_data->timer_running = 0;
		max_delay = DEFAULT_MAX_DELAY;
	}
	rtapi_mutex_give(&(rtapi_data->mutex));
#ifdef RTAPI_USE_PROCFS
	proc_clean();
#endif
	/* release master shared memory block */

	rtdm_free(rtapi_data);

	rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exit complete\n");
	return;
}

/***********************************************************************
 *                   GENERAL PURPOSE FUNCTIONS                          *
 ************************************************************************/

/* all RTAPI init is done when the rtapi kernel comp
 is insmoded.  The rtapi_init() and rtapi_exit() functions
 simply register that another comp is using the RTAPI.
 For other RTOSes, things might be different, especially
 if the RTOS does not use component. */

int rtapi_comp_init(const char *compname) {
	int n, comp_id;
	comp_data * comp;

	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: initing comp %s\n", compname);
	/* get the mutex */
	rtapi_mutex_get(&(rtapi_data->mutex));
	/* find empty spot in comp array */
	n = 1;
	while ((n <= RTAPI_MAX_COMP) && (comp_array[n].state != NO_COMP)) {
		n++;
	}
	if (n > RTAPI_MAX_COMP) {
		/* no room */
		rtapi_mutex_give(&(rtapi_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
				"RTAPI: ERROR: reached comp limit %d\n", n);
		return -EMFILE;
	}
	/* we have space for the comp */
	comp_id = n;
	comp = &(comp_array[n]);
	/* update comp data */
	comp->state = REALTIME;
	if (compname != NULL) {
		/* use name supplied by caller, truncating if needed */
		rtapi_snprintf(comp->name, RTAPI_NAME_LEN, "%s", compname);
	} else {
		/* make up a name */
		rtapi_snprintf(comp->name, RTAPI_NAME_LEN, "RTMOD%03d", comp_id);
	}
	rtapi_data->rt_comp_count++;

	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: comp '%s' loaded, ID: %d\n", comp->name, comp_id);
	rtapi_mutex_give(&(rtapi_data->mutex));

	return comp_id;
}


int rtapi_comp_exit(int comp_id) {
	int retval;

	rtapi_mutex_get(&(rtapi_data->mutex));
	retval = comp_delete(comp_id);
	rtapi_mutex_give(&(rtapi_data->mutex));

	return retval;
}

static int comp_delete(int comp_id) {
	comp_data * comp;
	char name[RTAPI_NAME_LEN + 1];
	int n;

	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: comp %d exiting\n", comp_id);
	/* validate comp ID */
	if ((comp_id < 1) || (comp_id > RTAPI_MAX_COMP)) {
		return -EINVAL;
	}

	/* point to the comp's data */
	comp = &(comp_array[comp_id]);
	/* check comp status */
	if (comp->state != REALTIME) {
		/* not an active realtime comp */
		return -EINVAL;
	}
	/* clean up any mess left behind by the comp */
	for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
		if ((task_array[n].state != EMPTY)
				&& (task_array[n].owner == comp_id)) {
			rtapi_print_msg(RTAPI_MSG_WARN,
					"RTAPI: WARNING: comp '%s' failed to delete task %02d\n",
					comp->name, n);
			task_delete(n);
		}
	}
	for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
		if (test_bit(comp_id, shmem_array[n].bitmap)) {
			rtapi_print_msg(RTAPI_MSG_WARN,
					"RTAPI: WARNING: comp '%s' failed to delete shmem %02d\n",
					comp->name, n);
			shmem_delete(n, comp_id);
		}
	}
	for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
		if (test_bit(comp_id, sem_array[n].bitmap)) {
			rtapi_print_msg(RTAPI_MSG_WARN,
					"RTAPI: WARNING: comp '%s' failed to delete sem %02d\n",
					comp->name, n);
			sem_delete(n, comp_id);
		}
	}

	/* use snprintf() to do strncpy(), since we don't have string.h */
	rtapi_snprintf(name, RTAPI_NAME_LEN, "%s", comp->name);
	/* update comp data */
	comp->state = NO_COMP;
	comp->name[0] = '\0';
	rtapi_data->rt_comp_count--;
	if (rtapi_data->rt_comp_count == 0) {
		if (rtapi_data->timer_running != 0) {

			rtapi_data->timer_period = 0;

			max_delay = DEFAULT_MAX_DELAY;
			rtapi_data->timer_running = 0;
		}
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: comp %d exited, name: '%s'\n",
			comp_id, name);
	return 0;
}

int rtapi_snprintf(char *buf, unsigned long int size, const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
	i = rtapi_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return i;
}

#define BUFFERLEN 1024

void default_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap) {
	static char buf[BUFFERLEN];
	rtapi_log_request_t *ring_req;

	rtapi_vsnprintf(buf, BUFFERLEN, fmt, ap);

	lprintk("%s", buf);

	/* Forward the message to the ring used by the user space, if enabled. */
	if (p_rtapi_log_enabled && (*p_rtapi_log_enabled)) {

		ring_req = RING_GET_REQUEST(&rtapi_log_ring, rtapi_log_ring.req_prod_pvt);

		/* Fill in the ring_req structure */
		strcpy(ring_req->line, buf);

		/* Make sure the other end "sees" the request when updating the index */
		mb();
		rtapi_log_ring.req_prod_pvt++;

		/* go... */
		RING_PUSH_REQUESTS(&rtapi_log_ring);
	}

}
static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

rtapi_msg_handler_t rtapi_get_msg_handler(void) {
	return rtapi_msg_handler;
}

void rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
	if (handler == NULL)
		rtapi_msg_handler = default_rtapi_msg_handler;
	else
		rtapi_msg_handler = handler;
}

void rtapi_print(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
	va_end(args);
}

void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
	va_list args;

	if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
		va_start(args, fmt);
		rtapi_msg_handler(level, fmt, args);
		va_end(args);
	}
}


int rtapi_set_msg_level(int level) {
	if ((level < RTAPI_MSG_NONE) || (level > RTAPI_MSG_ALL)) {
		return -EINVAL;
	}
	msg_level = level;
	return 0;
}

int rtapi_get_msg_level(void) {
	return msg_level;
}

/***********************************************************************
 *                     CLOCK RELATED FUNCTIONS                          *
 ************************************************************************/

long int rtapi_clock_set_period(long int nsecs) {

	/* opencn - We do not care about this period, we have 1 ns resolution which is pretty good :-) */

	rtapi_data->timer_period = xnclock_get_resolution();
	rtapi_data->timer_running = 1;

	return rtapi_data->timer_period;
}

long long int rtapi_get_time(void) {
	return rtdm_clock_read();
}


/* This returns a result in clocks instead of nS, and needs to be used
 with care around CPUs that change the clock speed to save power and
 other disgusting, non-realtime oriented behavior.  But at least it
 doesn't take a week every time you call it.
 *
 *** Warning !! opencn - return the value of the monotonic clock in ns
 */

long long int rtapi_get_clocks(void) {
	long long int retval;

	retval = rtdm_clock_read();

	return retval;
}


void rtapi_delay(long int nsec) {
	if (nsec > max_delay) {
		nsec = max_delay;
	}

	rtdm_task_busy_sleep(nsec);
}

long int rtapi_delay_max(void) {
	return max_delay;
}

/***********************************************************************
 *                     TASK RELATED FUNCTIONS                           *
 ************************************************************************/

/* Priority functions.  opencn uses 0 as the lowest priority, as the
 number decreases, the actual priority of the task decreases. */

int rtapi_prio_highest(void) {
	return MAX_RT_PRIO;
}

int rtapi_prio_lowest(void) {
	return 0;
}

int rtapi_prio_next_higher(int prio) {
	/* return a valid priority for out of range arg */
	if (prio > rtapi_prio_highest()) {
		return rtapi_prio_highest();
	}
	if (prio < rtapi_prio_lowest()) {
		return rtapi_prio_lowest();
	}

	/* return next higher priority for in-range arg */
	return prio + 1;
}

int rtapi_prio_next_lower(int prio) {
	/* return a valid priority for out of range arg */
	if (prio < rtapi_prio_lowest()) {
		return rtapi_prio_lowest();
	}
	if (prio > rtapi_prio_highest()) {
		return rtapi_prio_highest();
	}
	/* return next lower priority for in-range arg */
	return prio - 1;
}

/* We define taskcode as taking a void pointer and returning void, but
 rtai wants it to take an int and return void.
 We solve this with a wrapper function that meets rtai's needs.
 The wrapper functions also properly deals with tasks that return.
 (Most tasks are infinite loops, and don't return.)
 */

static void wrapper(void *args) {
	task_data *task;
	long task_id;

	task_id = *((long *) args);
	kfree(args);

	/* point to the task data */
	task = &task_array[task_id];

	/* call the task function with the task argument */
	(task->taskcode)(task->arg);

	/* if the task ever returns, we record that fact */
	task->state = ENDED;
}


int rtapi_task_new(void (*taskcode)(void *), void *arg, int prio, int owner, int uses_fp) {
	int n;
	long task_id;
	task_data *task;

	/* get the mutex */
	rtapi_mutex_get(&(rtapi_data->mutex));

	/* validate owner */
	if ((owner < 1) || (owner > RTAPI_MAX_COMP)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	if (comp_array[owner].state != REALTIME) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	/* find empty spot in task array */
	n = 1;
	while ((n <= RTAPI_MAX_TASKS) && (task_array[n].state != EMPTY)) {
		n++;
	}
	if (n > RTAPI_MAX_TASKS) {
		/* no room */
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EMFILE;
	}
	/* we have space for the task */
	task_id = n;
	task = &(task_array[n]);

	/* check requested priority */
	if ((prio < rtapi_prio_lowest()) || (prio > rtapi_prio_highest())) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	/* get space for the OS's task data - this is around 900 bytes, */
	/* so we don't want to statically allocate it for unused tasks. */
	ostask_array[task_id] = kmalloc(sizeof(rtdm_task_t), GFP_ATOMIC);
	if (ostask_array[task_id] == NULL) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -ENOMEM;
	}
	task->taskcode = taskcode;
	task->arg = arg;

	/* opencn - The rtdm task will be initialized and started in rtapi_task_start(). */

	/* the task has been created, update data */
	task->state = PAUSED;
	task->prio = prio;
	task->owner = owner;
	task->taskcode = taskcode;
	rtapi_data->task_count++;

	/* announce the birth of a brand new baby task */
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: task %02ld installed by comp %02d, priority %d, code: %p\n",
			task_id, task->owner, task->prio, taskcode);
	/* and return the ID to the proud parent */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return task_id;
}

/*
 * opencn - Deleting a task must require the commitment of this task which needs
 * to use  rtdm_task_should_stop() to react correctly.
 */
int rtapi_task_delete(int task_id) {
	int retval;

	rtapi_mutex_get(&(rtapi_data->mutex));
	retval = task_delete(task_id);
	rtapi_mutex_give(&(rtapi_data->mutex));
	return retval;
}


/*
 * opencn - Deleting a task must require the commitment of this task which needs
 * to use  rtdm_task_should_stop() to react correctly.
 */
static int task_delete(int task_id) {
	task_data *task;

	/* validate task ID */
	if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
		return -EINVAL;
	}
	/* point to the task's data */
	task = &(task_array[task_id]);
	/* check task status */
	if (task->state == EMPTY) {
		/* nothing to delete */
		return -EINVAL;
	}
	if ((task->state == PERIODIC) || (task->state == FREERUN)) {
		/* task is running, need to stop it */
		rtapi_print_msg(RTAPI_MSG_WARN,
				"RTAPI: WARNING: tried to delete task %02d while running\n",
				task_id);
		rtapi_task_pause(task_id);
	}

	/* get rid of it */

	rtdm_task_destroy(ostask_array[task_id]);

	/* free kernel memory */
	kfree(ostask_array[task_id]);

	/* update data */
	task->state = EMPTY;
	task->prio = 0;
	task->owner = 0;
	task->taskcode = NULL;
	ostask_array[task_id] = NULL;
	rtapi_data->task_count--;
	/* if no more tasks, stop the timer */
	if (rtapi_data->task_count == 0) {
		if (rtapi_data->timer_running != 0) {

			rtapi_data->timer_period = 0;
			max_delay = DEFAULT_MAX_DELAY;
			rtapi_data->timer_running = 0;
		}
	}
	/* done */
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: task %02d deleted\n", task_id);
	return 0;
}

int rtapi_task_start(int task_id, unsigned long int period_nsec) {
	int retval;
	task_data *task;
	char taskname[XNOBJECT_NAME_LEN];
	long *arg_task_id;

	/* validate task ID */
	if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
		return -EINVAL;
	}
	/* point to the task's data */
	task = &(task_array[task_id]);
	/* is task ready to be started? */
	if (task->state != PAUSED) {
		return -EINVAL;
	}
	/* can't start periodic tasks if timer isn't running */
	if ((rtapi_data->timer_running == 0) || (rtapi_data->timer_period == 0)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"RTAPI: could not start task: timer isn't running\n");
		return -EINVAL;
	}

	sprintf(taskname, "rtapi-task-%d", task_id);
	arg_task_id = (long *) kmalloc(sizeof(long), GFP_ATOMIC);
	if (!arg_task_id)
		BUG();
	*arg_task_id = task_id;

	/* The task must wait until rtapi_task_start() is called. */
	retval = rtdm_task_init(ostask_array[task_id], taskname, wrapper, arg_task_id, task->prio, period_nsec);
	if (retval != 0) {
		/* couldn't create task, free task data memory */
		kfree(ostask_array[task_id]);
		rtapi_mutex_give(&(rtapi_data->mutex));
		if (retval == ENOMEM) {
			/* not enough space for stack */
			return -ENOMEM;
		}
		/* unknown error */
		return -EINVAL;
	}

	/* ok, task is started */
	task->state = PERIODIC;
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: start_task id: %02d\n", task_id);
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: period_nsec: %ld\n", period_nsec);

	return retval;
}

void rtapi_wait(void) {
	int result;

	result = rtdm_task_wait_period(NULL);

	if (result != 0) {
		static int error_printed = 0;
		if (error_printed < 10) {
#warning still must adapt return of wait_period...
#ifdef RTE_TMROVRN
			if (result == RTE_TMROVRN) {
				rtapi_print_msg(
						error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
						"RTAPI: ERROR: Unexpected realtime delay on task %d\n"
						"This Message will only display once per session.\n"
						"Run the Latency Test and resolve before continuing.\n",
						rtapi_task_self());
			} else
#endif
#ifdef RTE_UNBLKD
			if (result == RTE_UNBLKD) {
				rtapi_print_msg(
						error_printed == 0 ? RTAPI_MSG_ERR : RTAPI_MSG_WARN,
						"RTAPI: ERROR: rt_task_wait_period() returned RTE_UNBLKD (%d).\n", result);
			} else
#endif
			{
				rtapi_print_msg(
						error_printed == 0 ?
								RTAPI_MSG_ERR :
								RTAPI_MSG_WARN,
						"RTAPI: ERROR: rt_task_wait_period() returned %d.\n",
						result);
			}
			error_printed++;
			if (error_printed == 10)
				rtapi_print_msg(
						error_printed == 0 ?
								RTAPI_MSG_ERR :
								RTAPI_MSG_WARN,
						"RTAPI: (further messages will be suppressed)\n");
		}
	}
}

int rtapi_task_resume(int task_id) {
	int retval;
	task_data *task;

	/* validate task ID */
	if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
		return -EINVAL;
	}
	/* point to the task's data */
	task = &(task_array[task_id]);
	/* is task ready to be started? */
	if (task->state != PAUSED) {
		return -EINVAL;
	}
	/* start the task */
	retval = rtdm_task_unblock(ostask_array[task_id]);
	if (retval != 0) {
		return -EINVAL;
	}
	/* update task data */
	task->state = FREERUN;
	return 0;
}

/*
 * opencn - Only the current reunning task can pause itself.
 * If a specific task should be aborted, use rtapi_task_delete().
 */
int rtapi_task_pause(int task_id) {
	int retval;
	int oldstate;
	task_data *task;

	/* opencn - Pausing a thread other than ourself is a *nosense* situation and
	 * should require synchronization mechanisms.
	 */
	if (task_id != rtapi_task_self())
		BUG();

	/* validate task ID */
	if ((task_id < 1) || (task_id > RTAPI_MAX_TASKS)) {
		return -EINVAL;
	}
	/* point to the task's data */
	task = &(task_array[task_id]);
	/* is it running? */
	if ((task->state != PERIODIC) && (task->state != FREERUN)) {
		return -EINVAL;
	}
	/* pause the task */
	oldstate = task->state;
	task->state = PAUSED;

	/* Suspend the task and until rtdm_unblock() is called. In this context,
	 * the return value is -EINTR.
	 */
	retval = rtdm_task_sleep(RTDM_TIMEOUT_INFINITE);
	if ((retval != -EINTR) && (retval != 0)) {
		task->state = oldstate;
		return -EINVAL;
	}
	/* update task data */
	return 0;
}


int rtapi_task_self(void) {

	rtdm_task_t *ptr;
	int n;

	/* ask OS for pointer to its data for the current task */
	ptr = rtdm_task_current();
	if (ptr == NULL) {
		/* called from outside a task? */
		return -EINVAL;
	}
	/* find matching entry in task array */
	n = 1;
	while (n <= RTAPI_MAX_TASKS) {
		if (ostask_array[n] == ptr) {
			/* found a match */
			return n;
		}
		n++;
	}
	return -EINVAL;
}

/***********************************************************************
 *                  SHARED MEMORY RELATED FUNCTIONS                     *
 ************************************************************************/

int rtapi_shmem_new(int key, int comp_id, unsigned long int size) {
	int n;
	int shmem_id;
	shmem_data *shmem;

	/* key must be non-zero, and also cannot match the key that RTAPI uses */
	if ((key == 0) || (key == RTAPI_KEY)) {
		return -EINVAL;
	}
	/* get the mutex */
	rtapi_mutex_get(&(rtapi_data->mutex));
	/* validate comp_id */
	if ((comp_id < 1) || (comp_id > RTAPI_MAX_COMP)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	if (comp_array[comp_id].state != REALTIME) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	/* check if a block is already open for this key */
	for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
		if (shmem_array[n].key == key) {
			/* found a match */

			shmem_id = n;
			shmem = &(shmem_array[n]);

			/* is it big enough? */
			if (shmem->size < size) {
				rtapi_mutex_give(&(rtapi_data->mutex));
				return -EINVAL;
			}

			/* yes, has it been mapped into kernel space? */
			if (shmem->rtusers == 0) {

				/* no, map it and save the address */
				/* Make sure we get page-aligned contiguous pages */
				shmem_addr_array[shmem_id] = (void *) __get_free_pages(GFP_ATOMIC, get_order(shmem->size));
				BUG_ON(!shmem_addr_array[shmem_id]);

			}
			/* is this comp already using it? */
			if (test_bit(comp_id, shmem->bitmap)) {
				rtapi_mutex_give(&(rtapi_data->mutex));
				return -EINVAL;
			}
			/* update usage data */
			set_bit(comp_id, shmem->bitmap);
			shmem->rtusers++;

			/* announce another user for this shmem */
			rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d opened by comp %02d\n", shmem_id, comp_id);

			rtapi_mutex_give(&(rtapi_data->mutex));
			return shmem_id;
		}
	}
	/* find empty spot in shmem array */
	n = 1;
	while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
		n++;
	}
	if (n > RTAPI_MAX_SHMEMS) {
		/* no room */
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EMFILE;
	}
	/* we have space for the block data */
	shmem_id = n;
	shmem = &(shmem_array[n]);

	/* get shared memory block from OS and save its address */
	shmem_addr_array[shmem_id] = (void *) __get_free_pages(GFP_ATOMIC, get_order(size));
	BUG_ON(!shmem_addr_array[shmem_id]);

	/* Initialize to 0 */
	memset(shmem_addr_array[shmem_id], 0, size);

	if (shmem_addr_array[shmem_id] == NULL) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -ENOMEM;
	}
	/* the block has been created, update data */
	set_bit(comp_id, shmem->bitmap);
	shmem->key = key;
	shmem->rtusers = 1;
	shmem->ulusers = 0;
	shmem->size = size;
	rtapi_data->shmem_count++;

	/* announce the birth of a brand new baby shmem */
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d created by comp %02d, key: 0x%x, size: %lu\n", shmem_id, comp_id, key, size);

	/* and return the ID to the proud parent */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return shmem_id;
}

int rtapi_shmem_delete(int shmem_id, int comp_id) {
	int retval;

	rtapi_mutex_get(&(rtapi_data->mutex));
	retval = shmem_delete(shmem_id, comp_id);
	rtapi_mutex_give(&(rtapi_data->mutex));
	return retval;
}

static int shmem_delete(int shmem_id, int comp_id) {
	shmem_data *shmem;

	/* validate shmem ID */
	if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
		return -EINVAL;
	}
	/* point to the shmem's data */
	shmem = &(shmem_array[shmem_id]);
	/* is the block valid? */
	if (shmem->key == 0) {
		return -EINVAL;
	}
	/* validate comp_id */
	if ((comp_id < 1) || (comp_id > RTAPI_MAX_COMP)) {
		return -EINVAL;
	}
	if (comp_array[comp_id].state != REALTIME) {
		return -EINVAL;
	}
	/* is this comp using the block? */
	if (test_bit(comp_id, shmem->bitmap) == 0) {
		return -EINVAL;
	}
	/* OK, we're no longer using it */
	clear_bit(comp_id, shmem->bitmap);
	shmem->rtusers--;
	/* is somebody else still using the block? */
	if (shmem->rtusers > 0) {
		/* yes, we're done for now */
		rtapi_print_msg(RTAPI_MSG_DBG,
				"RTAPI: shmem %02d closed by comp %02d\n",
				shmem_id, comp_id);
		return 0;
	}
	/* no other realtime users, free the shared memory from kernel space */

	free_pages((unsigned long) shmem_addr_array[shmem_id], get_order(shmem->size));

	shmem_addr_array[shmem_id] = NULL;
	shmem->rtusers = 0;
	/* are any user processes using the block? */
	if (shmem->ulusers > 0) {
		/* yes, we're done for now */
		rtapi_print_msg(RTAPI_MSG_DBG,
				"RTAPI: shmem %02d unmapped by comp %02d\n",
				shmem_id, comp_id);
		return 0;
	}
	/* no other users at all, this ID is now free */
	/* update the data array and usage count */
	shmem->key = 0;
	shmem->size = 0;
	rtapi_data->shmem_count--;
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: shmem %02d freed by comp %02d\n", shmem_id,
			comp_id);
	return 0;
}

int rtapi_shmem_getptr(int shmem_id, void **ptr) {
	/* validate shmem ID */
	if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
		return -EINVAL;
	}
	/* is the block mapped? */
	if (shmem_addr_array[shmem_id] == NULL) {
		return -EINVAL;
	}
	/* pass memory address back to caller */
	*ptr = shmem_addr_array[shmem_id];

	return 0;
}


/***********************************************************************
 *                    SEMAPHORE RELATED FUNCTIONS                       *
 ************************************************************************/

int rtapi_sem_new(int key, int comp_id) {
	int n;
	int sem_id;
	sem_data *sem;

	/* key must be non-zero */
	if (key == 0) {
		return -EINVAL;
	}
	/* get the mutex */
	rtapi_mutex_get(&(rtapi_data->mutex));
	/* validate comp_id */
	if ((comp_id < 1) || (comp_id > RTAPI_MAX_COMP)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	if (comp_array[comp_id].state != REALTIME) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	}
	/* check if a semaphore already exists for this key */
	for (n = 1; n <= RTAPI_MAX_SEMS; n++) {
		if ((sem_array[n].users > 0) && (sem_array[n].key == key)) {
			/* found a match */
			sem_id = n;
			sem = &(sem_array[n]);
			/* is this comp already using it? */
			if (test_bit(comp_id, sem->bitmap)) {
				/* yes, can't open it again */
				rtapi_mutex_give(&(rtapi_data->mutex));
				return -EINVAL;
			}
			/* update usage data */
			set_bit(comp_id, sem->bitmap);
			sem->users++;
			/* announce another user for this semaphore */
			rtapi_print_msg(RTAPI_MSG_DBG,
					"RTAPI: sem %02d opened by comp %02d\n",
					sem_id, comp_id);
			rtapi_mutex_give(&(rtapi_data->mutex));
			return sem_id;
		}
	}
	/* find empty spot in sem array */
	n = 1;
	while ((n <= RTAPI_MAX_SEMS) && (sem_array[n].users != 0)) {
		n++;
	}
	if (n > RTAPI_MAX_SEMS) {
		/* no room */
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EMFILE;
	}
	/* we have space for the semaphore */
	sem_id = n;
	sem = &(sem_array[n]);

	/* ask the OS to initialize the semaphore */

	rtdm_sem_init(&(ossem_array[sem_id]), 0);

	/* the semaphore has been created, update data */
	set_bit(comp_id, sem->bitmap);
	sem->users = 1;
	sem->key = key;
	rtapi_data->sem_count++;
	/* announce the birth of a brand new baby semaphore */
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: sem %02d created by comp %02d, key: %d\n",
			sem_id, comp_id, key);
	/* and return the ID to the proud parent */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return sem_id;
}

int rtapi_sem_delete(int sem_id, int comp_id) {
	int retval;

	rtapi_mutex_get(&(rtapi_data->mutex));
	retval = sem_delete(sem_id, comp_id);
	rtapi_mutex_give(&(rtapi_data->mutex));
	return retval;
}

static int sem_delete(int sem_id, int comp_id) {
	sem_data *sem;

	/* validate sem ID */
	if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
		return -EINVAL;
	}
	/* point to the semaphores's data */
	sem = &(sem_array[sem_id]);
	/* is the semaphore valid? */
	if (sem->users == 0) {
		return -EINVAL;
	}
	/* validate comp_id */
	if ((comp_id < 1) || (comp_id > RTAPI_MAX_COMP)) {
		return -EINVAL;
	}
	if (comp_array[comp_id].state != REALTIME) {
		return -EINVAL;
	}
	/* is this comp using the semaphore? */
	if (test_bit(comp_id, sem->bitmap) == 0) {
		return -EINVAL;
	}
	/* OK, we're no longer using it */
	clear_bit(comp_id, sem->bitmap);
	sem->users--;
	/* is somebody else still using the semaphore */
	if (sem->users > 0) {
		/* yes, we're done for now */
		rtapi_print_msg(RTAPI_MSG_DBG,
				"RTAPI: sem %02d closed by comp %02d\n",
				sem_id, comp_id);
		return 0;
	}

	/* no other users, ask the OS to shut down the semaphore */

	rtdm_sem_destroy(&(ossem_array[sem_id]));

	/* update the data array and usage count */
	sem->users = 0;
	sem->key = 0;
	rtapi_data->sem_count--;
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: sem %02d deleted by comp %02d\n", sem_id,
			comp_id);
	return 0;
}

int rtapi_sem_give(int sem_id) {
	sem_data *sem;

	/* validate sem ID */
	if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
		return -EINVAL;
	}
	/* point to the semaphores's data */
	sem = &(sem_array[sem_id]);
	/* is the semaphore valid? */
	if (sem->users == 0) {
		return -EINVAL;
	}
	/* give up the semaphore */

	rtdm_sem_up(&(ossem_array[sem_id]));

	return 0;
}

int rtapi_sem_take(int sem_id) {
	sem_data *sem;

	/* validate sem ID */
	if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
		return -EINVAL;
	}
	/* point to the semaphores's data */
	sem = &(sem_array[sem_id]);
	/* is the semaphore valid? */
	if (sem->users == 0) {
		return -EINVAL;
	}
	/* get the semaphore */

	rtdm_sem_down(&(ossem_array[sem_id]));
	return 0;
}

int rtapi_sem_try(int sem_id) {
	sem_data *sem;
	int retval;

	/* validate sem ID */
	if ((sem_id < 1) || (sem_id > RTAPI_MAX_SEMS)) {
		return -EINVAL;
	}
	/* point to the semaphores's data */
	sem = &(sem_array[sem_id]);
	/* is the semaphore valid? */
	if (sem->users == 0) {
		return -EINVAL;
	}
	/* try the semaphore */

	retval = rtdm_sem_timeddown(&(ossem_array[sem_id]), RTDM_TIMEOUT_NONE, NULL);
	if (retval == -EWOULDBLOCK)
		return -EBUSY;
	BUG_ON(retval < 0);

	return 0;
}

void rtapi_printall(void) {
	comp_data *comps;
	task_data *tasks;
	shmem_data *shmems;
	sem_data *sems;
	fifo_data *fifos;
	irq_data *irqs;
	int n, m;

	if (rtapi_data == NULL) {
		lprintk("rtapi_data = NULL, not initialized\n");
		return;
	}
	lprintk("rtapi_data = %p\n", rtapi_data);
	lprintk("  magic = %d\n", rtapi_data->magic);
	lprintk("  rev_code = %08x\n", rtapi_data->rev_code);
	lprintk("  mutex = %lu\n", rtapi_data->mutex);
	lprintk("  rt_comp_count = %d\n", rtapi_data->rt_comp_count);
	lprintk("  ul_comp_count = %d\n", rtapi_data->ul_comp_count);
	lprintk("  task_count  = %d\n", rtapi_data->task_count);
	lprintk("  shmem_count = %d\n", rtapi_data->shmem_count);
	lprintk("  sem_count   = %d\n", rtapi_data->sem_count);
	lprintk("  fifo_count  = %d\n", rtapi_data->fifo_count);
	lprintk("  irq_countc  = %d\n", rtapi_data->irq_count);
	lprintk("  timer_running = %d\n", rtapi_data->timer_running);
	lprintk("  timer_period  = %ld\n", rtapi_data->timer_period);
	comps = &(rtapi_data->comp_array[0]);
	tasks = &(rtapi_data->task_array[0]);
	shmems = &(rtapi_data->shmem_array[0]);
	sems = &(rtapi_data->sem_array[0]);
	fifos = &(rtapi_data->fifo_array[0]);
	irqs = &(rtapi_data->irq_array[0]);
	lprintk("  comp array = %p\n", comps);
	lprintk("  task array   = %p\n", tasks);
	lprintk("  shmem array  = %p\n", shmems);
	lprintk("  sem array    = %p\n", sems);
	lprintk("  fifo array   = %p\n", fifos);
	lprintk("  irq array    = %p\n", irqs);
	for (n = 0; n <= RTAPI_MAX_COMP; n++) {
		if (comps[n].state != NO_COMP) {
			lprintk("  comp %02d\n", n);
			lprintk("    state = %d\n", comps[n].state);
			lprintk("    name = %p\n", comps[n].name);
			lprintk("    name = '%s'\n", comps[n].name);
		}
	}
	for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
		if (tasks[n].state != EMPTY) {
			lprintk("  task %02d\n", n);
			lprintk("    state = %d\n", tasks[n].state);
			lprintk("    prio  = %d\n", tasks[n].prio);
			lprintk("    owner = %d\n", tasks[n].owner);
			lprintk("    code  = %p\n", tasks[n].taskcode);
		}
	}
	for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
		if (shmems[n].key != 0) {
			lprintk("  shmem %02d\n", n);
			lprintk("    key     = %d\n", shmems[n].key);
			lprintk("    rtusers = %d\n", shmems[n].rtusers);
			lprintk("    ulusers = %d\n", shmems[n].ulusers);
			lprintk("    size    = %ld\n", shmems[n].size);
			lprintk("    bitmap  = ");
			for (m = 0; m <= RTAPI_MAX_COMP; m++) {
				if (test_bit(m, shmems[n].bitmap)) {
					lprintk("%c", '1');
				} else {
					lprintk("%c", '0');
				}
			}
			lprintk("\n");
		}
	}
	for (n = 0; n <= RTAPI_MAX_SEMS; n++) {
		if (sems[n].key != 0) {
			lprintk("  sem %02d\n", n);
			lprintk("    key     = %d\n", sems[n].key);
			lprintk("    users   = %d\n", sems[n].users);
			lprintk("    bitmap  = ");
			for (m = 0; m <= RTAPI_MAX_COMP; m++) {
				if (test_bit(m, sems[n].bitmap)) {
					lprintk("%c", '1');
				} else {
					lprintk("%c", '0');
				}
			}
			lprintk("\n");
		}
	}
	for (n = 0; n <= RTAPI_MAX_FIFOS; n++) {
		if (fifos[n].state != UNUSED) {
			lprintk("  fifo %02d\n", n);
			lprintk("    state  = %d\n", fifos[n].state);
			lprintk("    key    = %d\n", fifos[n].key);
			lprintk("    reader = %d\n", fifos[n].reader);
			lprintk("    writer = %d\n", fifos[n].writer);
			lprintk("    size   = %ld\n", fifos[n].size);
		}
	}
	for (n = 0; n <= RTAPI_MAX_IRQS; n++) {
		if (irqs[n].irq_num != 0) {
			lprintk("  irq %02d\n", n);
			lprintk("    irq_num = %d\n", irqs[n].irq_num);
			lprintk("    owner   = %d\n", irqs[n].owner);
			lprintk("    handler = %p\n", irqs[n].handler);
		}
	}
}

/***********************************************************************
 *                        I/O RELATED FUNCTIONS                         *
 ************************************************************************/

void rtapi_outb(unsigned char byte, unsigned int port) {
	outb(byte, port);
}

unsigned char rtapi_inb(unsigned int port) {
	return inb(port);
}

int rtapi_is_realtime() {
	return 1;
}
int rtapi_is_kernelspace() {
	return 1;
}

/* starting with kernel 2.6, symbols that are used by other comps
 _must_ be explicitly exported.  2.4 and earlier kernels exported
 all non-static global symbols by default, so these explicit exports
 were not needed.  For 2.4 and older, you should define EXPORT_SYMTAB
 (before including comp.h) to make these explicit exports work and
 minimize pollution of the kernel namespace.  But EXPORT_SYMTAB
 must not be defined for 2.6, so the best place to do it is
 probably in the makefiles somewhere (as a -D option to gcc).
 */

EXPORT_SYMBOL(rtapi_snprintf);
EXPORT_SYMBOL(rtapi_vsnprintf);
EXPORT_SYMBOL(rtapi_print);
EXPORT_SYMBOL(rtapi_print_msg);
EXPORT_SYMBOL(rtapi_set_msg_level);
EXPORT_SYMBOL(rtapi_get_msg_level);
EXPORT_SYMBOL(rtapi_set_msg_handler);
EXPORT_SYMBOL(rtapi_get_msg_handler);
EXPORT_SYMBOL(rtapi_clock_set_period);
EXPORT_SYMBOL(rtapi_get_time);
EXPORT_SYMBOL(rtapi_get_clocks);
EXPORT_SYMBOL(rtapi_delay);
EXPORT_SYMBOL(rtapi_delay_max);
EXPORT_SYMBOL(rtapi_prio_highest);
EXPORT_SYMBOL(rtapi_prio_lowest);
EXPORT_SYMBOL(rtapi_prio_next_higher);
EXPORT_SYMBOL(rtapi_prio_next_lower);
EXPORT_SYMBOL(rtapi_task_new);
EXPORT_SYMBOL(rtapi_task_delete);
EXPORT_SYMBOL(rtapi_task_start);
EXPORT_SYMBOL(rtapi_wait);
EXPORT_SYMBOL(rtapi_task_resume);
EXPORT_SYMBOL(rtapi_task_pause);
EXPORT_SYMBOL(rtapi_task_self);
EXPORT_SYMBOL(rtapi_shmem_new);
EXPORT_SYMBOL(rtapi_shmem_delete);
EXPORT_SYMBOL(rtapi_shmem_getptr);
EXPORT_SYMBOL(rtapi_sem_new);
EXPORT_SYMBOL(rtapi_sem_delete);
EXPORT_SYMBOL(rtapi_sem_give);
EXPORT_SYMBOL(rtapi_sem_take);
EXPORT_SYMBOL(rtapi_sem_try);

EXPORT_SYMBOL(rtapi_outb);
EXPORT_SYMBOL(rtapi_inb);
EXPORT_SYMBOL(rtapi_is_realtime);
EXPORT_SYMBOL(rtapi_is_kernelspace);

subsys_initcall(rtapi_init)
