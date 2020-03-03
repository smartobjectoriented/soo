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

#if defined(CONFIG_SOO_AGENCY)

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#include <soo/debug/time.h>
#include <soo/debug/bandwidth.h>

#include <arm_neon.h>

void ll_bandwidth_compute(s64 *delays, size_t size, uint32_t *div, uint32_t *result) {
	uint32_t i;
	s64 sum = 0;
	float div_f, result_f;

	for (i = 0 ; i < N_BANDWIDTH_DELAYS ; i++)
		sum += delays[i];

	div_f = (float) ((uint32_t) sum / N_BANDWIDTH_DELAYS);
	result_f = (float) size / (div_f * 1E-9); /* ns > s */
	result_f *= 8.0; /* B > bits */
	result_f /= 1E6; /* bps > Mbps */

	*div = (uint32_t) div_f;
	*result = (uint32_t) result_f;
}

#endif /* CONFIG_SOO_AGENCY */
