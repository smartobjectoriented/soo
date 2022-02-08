
#ifndef OPENCN_H
#define OPENCN_H

#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/jiffies.h>

#include <asm/atomic.h>

#ifndef RTDM_HZ
#define RTDM_HZ     CONFIG_RTDM_HZ
#endif /* RTDM_HZ */

extern unsigned long volatile __cacheline_aligned_in_smp __jiffy_arch_data rtdm_jiffies;


#endif /* OPENCN_H */

