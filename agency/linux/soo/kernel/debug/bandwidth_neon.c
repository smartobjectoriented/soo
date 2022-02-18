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

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#include <soo/debug/time.h>
#include <soo/debug/bandwidth.h>

#include <uapi/asm-generic/posix_types.h>

#ifdef CONFIG_ARM
#include <arm_neon.h>
#endif

void ll_bandwidth_compute(s64 delay, size_t size, uint32_t *div, uint32_t *result) {
	float div_f, result_f;
	s32 rem;
	__kernel_timer_t sec;

	sec = div_s64_rem(delay, NSEC_PER_SEC, &rem);
	if (unlikely(rem < 0)) {
		sec--;
		rem += NSEC_PER_SEC;
	}

	div_f = (float) sec;

	result_f = (float) size / div_f;
	result_f *= 8.0; /* B > bits */
	result_f /= 1E6; /* bps > Mbps */

	*div = (uint32_t) div_f;
	*result = (uint32_t) result_f;
}

