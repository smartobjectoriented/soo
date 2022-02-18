/*
 * Copyright (C) 2001-2008,2012 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2004,2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
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
#ifndef _COBALT_KERNEL_LOCK_H
#define _COBALT_KERNEL_LOCK_H

#include <linux/ipipe.h>
#include <linux/percpu.h>
#include <cobalt/kernel/assert.h>


#if XENO_DEBUG(LOCKING)

struct xnlock {
	unsigned owner;
	arch_spinlock_t alock;
	const char *file;
	const char *function;
	unsigned int line;
	int cpu;
	unsigned long long spin_time;
	unsigned long long lock_date;
};

struct xnlockinfo {
	unsigned long long spin_time;
	unsigned long long lock_time;
	const char *file;
	const char *function;
	unsigned int line;
};

#define XNARCH_LOCK_UNLOCKED (struct xnlock) {	\
	~0,					\
	__ARCH_SPIN_LOCK_UNLOCKED,		\
	NULL,					\
	NULL,					\
	0,					\
	-1,					\
	0LL,					\
	0LL,					\
}

#define XNLOCK_DBG_CONTEXT		, __FILE__, __LINE__, __FUNCTION__
#define XNLOCK_DBG_CONTEXT_ARGS					\
	, const char *file, int line, const char *function
#define XNLOCK_DBG_PASS_CONTEXT		, file, line, function

void xnlock_dbg_prepare_acquire(unsigned long long *start);
void xnlock_dbg_prepare_spin(unsigned int *spin_limit);
void xnlock_dbg_acquired(struct xnlock *lock, int cpu,
			 unsigned long long *start,
			 const char *file, int line,
			 const char *function);
int xnlock_dbg_release(struct xnlock *lock,
			 const char *file, int line,
			 const char *function);

DECLARE_PER_CPU(struct xnlockinfo, xnlock_stats);

#else /* !XENO_DEBUG(LOCKING) */

struct xnlock {
	unsigned owner;
	arch_spinlock_t alock;
};

#define XNARCH_LOCK_UNLOCKED			\
	(struct xnlock) {			\
		~0,				\
		__ARCH_SPIN_LOCK_UNLOCKED,	\
	}

#define XNLOCK_DBG_CONTEXT
#define XNLOCK_DBG_CONTEXT_ARGS
#define XNLOCK_DBG_PASS_CONTEXT

static inline
void xnlock_dbg_prepare_acquire(unsigned long long *start)
{
}

static inline
void xnlock_dbg_prepare_spin(unsigned int *spin_limit)
{
}

static inline void
xnlock_dbg_acquired(struct xnlock *lock, int cpu,
		    unsigned long long *start)
{
}

static inline int xnlock_dbg_release(struct xnlock *lock)
{
	return 0;
}

#endif /* !XENO_DEBUG(LOCKING) */

#define xnlock_get(lock)		__xnlock_get(lock  XNLOCK_DBG_CONTEXT)
#define xnlock_put(lock)		__xnlock_put(lock  XNLOCK_DBG_CONTEXT)
#define xnlock_get_irqsave(lock,x) \
	((x) = __xnlock_get_irqsave(lock  XNLOCK_DBG_CONTEXT))
#define xnlock_put_irqrestore(lock,x) \
	__xnlock_put_irqrestore(lock,x  XNLOCK_DBG_CONTEXT)

static inline void xnlock_init (struct xnlock *lock)
{
	*lock = XNARCH_LOCK_UNLOCKED;
}

#define DECLARE_XNLOCK(lock)		struct xnlock lock
#define DECLARE_EXTERN_XNLOCK(lock)	extern struct xnlock lock
#define DEFINE_XNLOCK(lock)		struct xnlock lock = XNARCH_LOCK_UNLOCKED
#define DEFINE_PRIVATE_XNLOCK(lock)	static DEFINE_XNLOCK(lock)

static inline int ____xnlock_get(struct xnlock *lock /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	int cpu = ipipe_processor_id();
	unsigned long long start;

	if (lock->owner == cpu)
		return 2;

	xnlock_dbg_prepare_acquire(&start);

	arch_spin_lock(&lock->alock);
	lock->owner = cpu;

	xnlock_dbg_acquired(lock, cpu, &start /*, */ XNLOCK_DBG_PASS_CONTEXT);

	return 0;
}

static inline void ____xnlock_put(struct xnlock *lock /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	if (xnlock_dbg_release(lock /*, */ XNLOCK_DBG_PASS_CONTEXT))
		return;

	lock->owner = ~0U;
	arch_spin_unlock(&lock->alock);
}

#define ___xnlock_get ____xnlock_get
#define ___xnlock_put ____xnlock_put


static inline unsigned long
__xnlock_get_irqsave(struct xnlock *lock /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	unsigned long flags;

	local_irq_save(flags);

	return flags;
}

static inline void __xnlock_put_irqrestore(struct xnlock *lock, unsigned long flags
					   /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	local_irq_restore(flags);
}

static inline int xnlock_is_owner(struct xnlock *lock)
{
	return lock->owner == ipipe_processor_id();
}

static inline int __xnlock_get(struct xnlock *lock /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	return ___xnlock_get(lock /* , */ XNLOCK_DBG_PASS_CONTEXT);
}

static inline void __xnlock_put(struct xnlock *lock /*, */ XNLOCK_DBG_CONTEXT_ARGS)
{
	___xnlock_put(lock /*, */ XNLOCK_DBG_PASS_CONTEXT);
}


DECLARE_EXTERN_XNLOCK(nklock);

/** @} */

#endif /* !_COBALT_KERNEL_LOCK_H */
