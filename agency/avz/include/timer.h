/******************************************************************************
 * timer.h
 * 
 * Copyright (c) 2002-2003 Rolf Neugebauer
 * Copyright (c) 2002-2005 K A Fraser
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <spinlock.h>
#include <percpu.h>
#include <time.h>
#include <string.h>

struct timer {
    /* System time expiry value (nanoseconds since boot). */
    u64 expires;

    /* Linked list. */
    struct timer *list_next;

    /* On expiry, '(*function)(data)' will be executed in softirq context. */
    void (*function)(void *);

    /* Some timer might be initialized on CPU #1 or #2, and in this case, they rely on the CPU #0 timer */
    struct timer *sibling;
    int timer_cpu;

    void *data;

    /* CPU on which this timer will be installed and executed. */
    uint16_t cpu;

    /* Timer status. */
#define TIMER_STATUS_inactive  0  /* Not in use; can be activated.    */
#define TIMER_STATUS_killed    1  /* Not in use; canot be activated.  */
#define TIMER_STATUS_in_list   2  /* In use; on overflow linked list. */

    uint8_t status;
};

/*
 * All functions below can be called for any CPU from any CPU in any context.
 */

/*
 * Returns TRUE if the given timer is on a timer list.
 * The timer must *previously* have been initialised by init_timer(), or its
 * structure initialised to all-zeroes.
 */
static inline int active_timer(struct timer *timer)
{
    return (timer->status == TIMER_STATUS_in_list);
}

/*
 * Initialise a timer structure with an initial callback CPU, callback
 * function and callback data pointer. This function may be called at any
 * time (and multiple times) on an inactive timer. It must *never* execute
 * concurrently with any other operation on the same timer.
 */
static inline void reset_timer(struct timer *timer, void (*function)(void *), void *data, unsigned int cpu)
{
    memset(timer, 0, sizeof(*timer));

    timer->function = function;
    timer->data     = data;
    timer->cpu      = cpu;
}

/*
 * Set the expiry time and activate a timer. The timer must *previously* have
 * been initialised by init_timer() (so that callback details are known).
 */
extern void set_timer(struct timer *timer, u64 expires);

/*
 * Deactivate a timer This function has no effect if the timer is not currently
 * active.
 * The timer must *previously* have been initialised by init_timer(), or its
 * structure initialised to all zeroes.
 */
extern void stop_timer(struct timer *timer);

/*
 * Deactivate a timer and prevent it from being re-set (future calls to
 * set_timer will silently fail). When this function returns it is guaranteed
 * that the timer callback handler is not running on any CPU.
 * The timer must *previously* have been initialised by init_timer(), or its
 * structure initialised to all zeroes.
 */
extern void kill_timer(struct timer *timer);

/*
 * Next timer deadline for each CPU.
 * Modified only by the local CPU and never in interrupt context.
 */
DECLARE_PER_CPU(u64, timer_deadline_start);
DECLARE_PER_CPU(u64, timer_deadline_end);

/* Arch-defined function to reprogram timer hardware for new deadline. */
extern void reprogram_timer(u64 deadline);

void timer_init(void);

#endif /* _TIMER_H_ */

