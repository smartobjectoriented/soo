/*
 * Copyright (C) 2014 Jan Kiszka <jan.kiszka@siemens.com>.
 * Copyright (C) 2014 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#undef TRACE_INCLUDE_PATH

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cobalt_rtdm
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE cobalt-rtdm

#if !defined(_TRACE_COBALT_RTDM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_COBALT_RTDM_H

#include <linux/tracepoint.h>
#include <linux/mman.h>
#include <linux/sched.h>

struct rtdm_event;
struct rtdm_sem;
struct rtdm_mutex;
struct xnthread;
struct rtdm_device;
struct rtdm_dev_context;

DECLARE_EVENT_CLASS(task_op,
	TP_PROTO(struct xnthread *task),
	TP_ARGS(task),

	TP_STRUCT__entry(
		__field(struct xnthread *, task)
		__string(task_name, task->name)
	),

	TP_fast_assign(
		__entry->task = task;
		__assign_str(task_name, task->name);
	),

	TP_printk("task %p(%s)", __entry->task, __get_str(task_name))
);

DECLARE_EVENT_CLASS(event_op,
	TP_PROTO(struct rtdm_event *ev),
	TP_ARGS(ev),

	TP_STRUCT__entry(
		__field(struct rtdm_event *, ev)
	),

	TP_fast_assign(
		__entry->ev = ev;
	),

	TP_printk("event=%p", __entry->ev)
);

DECLARE_EVENT_CLASS(sem_op,
	TP_PROTO(struct rtdm_sem *sem),
	TP_ARGS(sem),

	TP_STRUCT__entry(
		__field(struct rtdm_sem *, sem)
	),

	TP_fast_assign(
		__entry->sem = sem;
	),

	TP_printk("sem=%p", __entry->sem)
);

DECLARE_EVENT_CLASS(mutex_op,
	TP_PROTO(struct rtdm_mutex *mutex),
	TP_ARGS(mutex),

	TP_STRUCT__entry(
		__field(struct rtdm_mutex *, mutex)
	),

	TP_fast_assign(
		__entry->mutex = mutex;
	),

	TP_printk("mutex=%p", __entry->mutex)
);


DEFINE_EVENT(task_op, cobalt_driver_task_join,
	TP_PROTO(struct xnthread *task),
	TP_ARGS(task)
);

TRACE_EVENT(cobalt_driver_event_init,
	TP_PROTO(struct rtdm_event *ev, unsigned long pending),
	TP_ARGS(ev, pending),

	TP_STRUCT__entry(
		__field(struct rtdm_event *, ev)
		__field(unsigned long,	pending)
	),

	TP_fast_assign(
		__entry->ev = ev;
		__entry->pending = pending;
	),

	TP_printk("event=%p pending=%#lx",
		  __entry->ev, __entry->pending)
);

TRACE_EVENT(cobalt_driver_event_wait,
	TP_PROTO(struct rtdm_event *ev, struct xnthread *task),
	TP_ARGS(ev, task),

	TP_STRUCT__entry(
		__field(struct xnthread *, task)
		__string(task_name, task->name)
		__field(struct rtdm_event *, ev)
	),

	TP_fast_assign(
		__entry->task = task;
		__assign_str(task_name, task->name);
		__entry->ev = ev;
	),

	TP_printk("event=%p task=%p(%s)",
		  __entry->ev, __entry->task, __get_str(task_name))
);

DEFINE_EVENT(event_op, cobalt_driver_event_signal,
	TP_PROTO(struct rtdm_event *ev),
	TP_ARGS(ev)
);

DEFINE_EVENT(event_op, cobalt_driver_event_clear,
	TP_PROTO(struct rtdm_event *ev),
	TP_ARGS(ev)
);

DEFINE_EVENT(event_op, cobalt_driver_event_pulse,
	TP_PROTO(struct rtdm_event *ev),
	TP_ARGS(ev)
);

DEFINE_EVENT(event_op, cobalt_driver_event_destroy,
	TP_PROTO(struct rtdm_event *ev),
	TP_ARGS(ev)
);

TRACE_EVENT(cobalt_driver_sem_init,
	TP_PROTO(struct rtdm_sem *sem, unsigned long value),
	TP_ARGS(sem, value),

	TP_STRUCT__entry(
		__field(struct rtdm_sem *, sem)
		__field(unsigned long, value)
	),

	TP_fast_assign(
		__entry->sem = sem;
		__entry->value = value;
	),

	TP_printk("sem=%p value=%lu",
		  __entry->sem, __entry->value)
);

TRACE_EVENT(cobalt_driver_sem_wait,
	TP_PROTO(struct rtdm_sem *sem, struct xnthread *task),
	TP_ARGS(sem, task),

	TP_STRUCT__entry(
		__field(struct xnthread *, task)
		__string(task_name, task->name)
		__field(struct rtdm_sem *, sem)
	),

	TP_fast_assign(
		__entry->task = task;
		__assign_str(task_name, task->name);
		__entry->sem = sem;
	),

	TP_printk("sem=%p task=%p(%s)",
		  __entry->sem, __entry->task, __get_str(task_name))
);

DEFINE_EVENT(sem_op, cobalt_driver_sem_up,
	TP_PROTO(struct rtdm_sem *sem),
	TP_ARGS(sem)
);

DEFINE_EVENT(sem_op, cobalt_driver_sem_destroy,
	TP_PROTO(struct rtdm_sem *sem),
	TP_ARGS(sem)
);

DEFINE_EVENT(mutex_op, cobalt_driver_mutex_init,
	TP_PROTO(struct rtdm_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_op, cobalt_driver_mutex_release,
	TP_PROTO(struct rtdm_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_op, cobalt_driver_mutex_destroy,
	TP_PROTO(struct rtdm_mutex *mutex),
	TP_ARGS(mutex)
);

TRACE_EVENT(cobalt_driver_mutex_wait,
	TP_PROTO(struct rtdm_mutex *mutex, struct xnthread *task),
	TP_ARGS(mutex, task),

	TP_STRUCT__entry(
		__field(struct xnthread *, task)
		__string(task_name, task->name)
		__field(struct rtdm_mutex *, mutex)
	),

	TP_fast_assign(
		__entry->task = task;
		__assign_str(task_name, task->name);
		__entry->mutex = mutex;
	),

	TP_printk("mutex=%p task=%p(%s)",
		  __entry->mutex, __entry->task, __get_str(task_name))
);

#endif /* _TRACE_COBALT_RTDM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
