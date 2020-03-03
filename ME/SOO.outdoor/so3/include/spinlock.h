/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <asm/processor.h>
#include <asm/spinlock.h>


#ifndef NDEBUG
struct lock_debug {
    int irq_safe; /* +1: IRQ-safe; 0: not IRQ-safe; -1: don't know yet */
};
#define _LOCK_DEBUG { -1 }
void spin_debug_enable(void);
void spin_debug_disable(void);
#else
struct lock_debug { };
#define _LOCK_DEBUG { }
#define spin_debug_enable() ((void)0)
#define spin_debug_disable() ((void)0)
#endif


struct lock_profile { };
struct lock_profile_qhead { };

#define SPIN_LOCK_UNLOCKED                                                    \
    { _RAW_SPIN_LOCK_UNLOCKED, 0xfffu, 0, _LOCK_DEBUG, { } }
#define DEFINE_SPINLOCK(l) spinlock_t l = SPIN_LOCK_UNLOCKED

#define spin_lock_init_prof(s, l) spin_lock_init(&((s)->l))
#define lock_profile_register_struct(type, ptr, idx, print)
#define lock_profile_deregister_struct(type, ptr)

typedef struct {
    raw_spinlock_t raw;
    u16 recurse_cpu:12;
    u16 recurse_cnt:4;
    struct lock_debug debug;
    struct lock_profile profile;
} spinlock_t;


#define spin_lock_init(l) (*(l) = (spinlock_t)SPIN_LOCK_UNLOCKED)

typedef struct {
    raw_rwlock_t raw;
    struct lock_debug debug;
} rwlock_t;

#define RW_LOCK_UNLOCKED { _RAW_RW_LOCK_UNLOCKED, _LOCK_DEBUG }
#define DEFINE_RWLOCK(l) rwlock_t l = RW_LOCK_UNLOCKED
#define rwlock_init(l) (*(l) = (rwlock_t)RW_LOCK_UNLOCKED)

void _spin_lock(spinlock_t *lock);
void _spin_lock_irq(spinlock_t *lock);
uint32_t _spin_lock_irqsave(spinlock_t *lock);

void _spin_unlock(spinlock_t *lock);
void _spin_unlock_irq(spinlock_t *lock);
void _spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags);

int _spin_is_locked(spinlock_t *lock);
int _spin_trylock(spinlock_t *lock);
void _spin_barrier(spinlock_t *lock);
void _spin_barrier_irq(spinlock_t *lock);

void _spin_lock_recursive(spinlock_t *lock);
void _spin_unlock_recursive(spinlock_t *lock);

void _read_lock(rwlock_t *lock);
void _read_lock_irq(rwlock_t *lock);
uint32_t _read_lock_irqsave(rwlock_t *lock);

void _read_unlock(rwlock_t *lock);
void _read_unlock_irq(rwlock_t *lock);
void _read_unlock_irqrestore(rwlock_t *lock, uint32_t flags);

void _write_lock(rwlock_t *lock);
void _write_lock_irq(rwlock_t *lock);
uint32_t _write_lock_irqsave(rwlock_t *lock);
int _write_trylock(rwlock_t *lock);

void _write_unlock(rwlock_t *lock);
void _write_unlock_irq(rwlock_t *lock);
void _write_unlock_irqrestore(rwlock_t *lock, uint32_t flags);

int _rw_is_locked(rwlock_t *lock);
int _rw_is_write_locked(rwlock_t *lock);

#define spin_lock(l)                  _spin_lock(l)
#define spin_lock_irq(l)              _spin_lock_irq(l)
#define spin_lock_irqsave(l, f)       ((f) = _spin_lock_irqsave(l))

#define spin_unlock(l)                _spin_unlock(l)
#define spin_unlock_irq(l)            _spin_unlock_irq(l)
#define spin_unlock_irqrestore(l, f)  _spin_unlock_irqrestore(l, f)

#define spin_is_locked(l)             _spin_is_locked(l)
#define spin_trylock(l)               _spin_trylock(l)

/* Ensure a lock is quiescent between two critical operations. */
#define spin_barrier(l)               _spin_barrier(l)
#define spin_barrier_irq(l)           _spin_barrier_irq(l)

/*
 * spin_[un]lock_recursive(): Use these forms when the lock can (safely!) be
 * reentered recursively on the same CPU. All critical regions that may form
 * part of a recursively-nested set must be protected by these forms. If there
 * are any critical regions that cannot form part of such a set, they can use
 * standard spin_[un]lock().
 */
#define spin_lock_recursive(l)        _spin_lock_recursive(l)
#define spin_unlock_recursive(l)      _spin_unlock_recursive(l)

#define read_lock(l)                  _read_lock(l)
#define read_lock_irq(l)              _read_lock_irq(l)
#define read_lock_irqsave(l, f)       ((f) = _read_lock_irqsave(l))

#define read_unlock(l)                _read_unlock(l)
#define read_unlock_irq(l)            _read_unlock_irq(l)
#define read_unlock_irqrestore(l, f)  _read_unlock_irqrestore(l, f)

#define write_lock(l)                 _write_lock(l)
#define write_lock_irq(l)             _write_lock_irq(l)
#define write_lock_irqsave(l, f)      ((f) = _write_lock_irqsave(l))
#define write_trylock(l)              _write_trylock(l)

#define write_unlock(l)               _write_unlock(l)
#define write_unlock_irq(l)           _write_unlock_irq(l)
#define write_unlock_irqrestore(l, f) _write_unlock_irqrestore(l, f)

#define rw_is_locked(l)               _rw_is_locked(l)
#define rw_is_write_locked(l)         _rw_is_write_locked(l)

#endif /* SPINLOCK_H */

