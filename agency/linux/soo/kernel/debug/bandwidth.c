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

static s64 *delays[N_BANDWIDTH_SLOTS];
static s64 prev_timestamps[N_BANDWIDTH_SLOTS] = { 0 };
static uint32_t delays_count[N_BANDWIDTH_SLOTS] = { 0 };

int ll_bandwidth_collect_delay(uint32_t index) {
	int ret = 0;
	s64 timestamp;

	if (unlikely(prev_timestamps[index] == 0)) {
		prev_timestamps[index] = ll_time_get();
		return 0;
	}

	timestamp = ll_time_get();
	delays[index][delays_count[index]] = timestamp - prev_timestamps[index];
	prev_timestamps[index] = timestamp;

	if (unlikely(delays_count[index] == N_BANDWIDTH_DELAYS - 1))
		ret = 1;

	delays_count[index] = (delays_count[index] + 1) % N_BANDWIDTH_DELAYS;

	return ret;
}

void ll_bandwidth_collect_delay_show(uint32_t index, size_t size) {
	uint32_t div, result;

	if (ll_bandwidth_collect_delay(index)) {
		kernel_neon_begin();
		ll_bandwidth_compute(delays[index], size, &div, &result);
		kernel_neon_end();

		lprintk("%d: %dns, %dMBps\n", index, div, result);
	}
}

int rtdm_ll_bandwidth_collect_delay(uint32_t index) {
	int ret = 0;
	s64 timestamp;

	if (unlikely(prev_timestamps[index] == 0)) {
		prev_timestamps[index] = ll_time_get();
		return 0;
	}

	timestamp = ll_time_get();
	delays[index][delays_count[index]] = timestamp - prev_timestamps[index];
	prev_timestamps[index] = timestamp;

	if (unlikely(delays_count[index] == N_BANDWIDTH_DELAYS - 1))
		ret = 1;

	delays_count[index] = (delays_count[index] + 1) % N_BANDWIDTH_DELAYS;

	return ret;
}

void rtdm_ll_bandwidth_collect_delay_show(uint32_t index, size_t size) {
	uint32_t div, result;

	if (rtdm_ll_bandwidth_collect_delay(index)) {
		kernel_neon_begin();
		ll_bandwidth_compute(delays[index], size, &div, &result);
		kernel_neon_end();

		lprintk("%d: %dns, %dMBps\n", index, div, result);
	}
}

void ll_bandwidth_reset_delays(uint32_t index) {
	uint32_t i;

	for (i = 0 ; i < N_BANDWIDTH_DELAYS ; i++)
		delays[index][i] = 0;

	prev_timestamps[index] = 0;
	delays_count[index] = 0;
}

void ll_bandwidth_init(void) {
	uint32_t i;

	for (i = 0 ; i < N_BANDWIDTH_SLOTS ; i++) {
		delays[i] = kzalloc(N_BANDWIDTH_DELAYS * sizeof(s64), GFP_KERNEL);
		ll_bandwidth_reset_delays(i);
	}
}

