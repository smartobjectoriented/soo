/******************************************************************************
 * time.h
 * 
 * Copyright (c) 2002-2003 Rolf Neugebauer
 * Copyright (c) 2002-2005 K A Fraser
 */

#ifndef __AVZ_TIME_H__
#define __AVZ_TIME_H__

#include <avz/types.h>

#include <soo/uapi/avz.h>

#include <asm/time.h>

extern int init_time(void);

struct domain;

/*
 * System Time
 * 64 bit value containing the nanoseconds elapsed since boot time.
 * This value is adjusted by frequency drift.
 * NOW() returns the current time.
 * The other macros are for convenience to approximate short intervals
 * of real time into system time 
 */


u64 get_s_time(void);

#define NOW()           ((u64) get_s_time())
#define STIME_MAX 	((u64)(~0ull))

extern void update_vcpu_system_time(struct vcpu *v);

extern void do_settime(unsigned long secs, unsigned long nsecs, u64 system_time_base);

extern void send_timer_event(struct vcpu *v);
extern void send_timer_rt_event(struct vcpu *v);

void domain_set_time_offset(struct domain *d, int32_t time_offset_seconds);


#endif /* __AVZ_TIME_H__ */

