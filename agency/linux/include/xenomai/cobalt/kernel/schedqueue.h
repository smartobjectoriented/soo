/*
 * Copyright (C) 2008 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_KERNEL_SCHEDQUEUE_H
#define _COBALT_KERNEL_SCHEDQUEUE_H

#include <cobalt/kernel/list.h>

/**
 * @addtogroup cobalt_core_sched
 * @{
 */

#define XNSCHED_CLASS_WEIGHT_FACTOR	1024

typedef struct list_head xnsched_queue_t;

#define xnsched_initq(__q)			INIT_LIST_HEAD(__q)
#define xnsched_emptyq_p(__q)			list_empty(__q)
#define xnsched_addq(__q, __t)			list_add_prilf(__t, __q, cprio, rlink)
#define xnsched_addq_tail(__q, __t)		list_add_priff(__t, __q, cprio, rlink)
#define xnsched_delq(__q, __t)			(void)(__q), list_del(&(__t)->rlink)
#define xnsched_getq(__q)							\
	({									\
		struct xnthread *__t = NULL;					\
		if (!list_empty(__q))						\
			__t = list_get_entry(__q, struct xnthread, rlink);	\
		__t;								\
	})
#define xnsched_weightq(__q)						\
	({								\
		struct xnthread *__t;					\
		__t = list_first_entry(__q, struct xnthread, rlink);	\
		__t->cprio;						\
	})
	

struct xnthread *xnsched_findq(xnsched_queue_t *q, int prio);

/** @} */

#endif /* !_COBALT_KERNEL_SCHEDQUEUE_H */
