/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <asm/neon.h>

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#include <soo/debug/time.h>
#include <soo/debug/bandwidth.h>

static s64 delays[N_BANDWIDTH_SLOTS] = { 0 };
static s64 prev_timestamps[N_BANDWIDTH_SLOTS] = { 0 };

void ll_bandwidth_collect_delay(uint32_t index) {
	s64 timestamp;

	if (unlikely(prev_timestamps[index] == 0)) {
		prev_timestamps[index] = ll_time_get();
		return ;
	}

	timestamp = ll_time_get();
	delays[index] = timestamp - prev_timestamps[index];
	prev_timestamps[index] = timestamp;
}

void ll_bandwidth_show(uint32_t index, size_t size) {
#ifdef CONFIG_ARM
	uint32_t div, result;

	kernel_neon_begin();
	ll_bandwidth_compute(delays[index], size, &div, &result);
	kernel_neon_end();


	lprintk("%d: %lld ns, %dMBps\n", index, delays[index], result);
#endif /* CONFIG_ARM */
}


void ll_bandwidth_reset_delays(uint32_t index) {
	delays[index] = 0;
	prev_timestamps[index] = 0;
}

