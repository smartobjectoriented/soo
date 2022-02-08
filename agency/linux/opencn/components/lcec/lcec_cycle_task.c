/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifdef CONFIG_ARM
#include <asm/neon.h>
#endif

/* Xenomai */
#include <xenomai/rtdm/driver.h>

#include <opencn/rtapi/rtapi_mutex.h>

#include <soo/uapi/console.h>

#include <opencn/uapi/lcec.h>

#include "lcec_priv.h"

void clock_correction_init_fp(struct clock_correction* corr, uint64_t task_period_ns)
{
	corr->limit_ns = do_div(task_period_ns, 1000);
	corr->period_ns = task_period_ns;
	corr->isum_ns = 0ll;
	corr->out_ns = 0ll;

	corr->pll_p = 0.005;
	corr->pll_i = 0.01;
	corr->periodfp = (uint32_t)task_period_ns * 0.000000001;
}

static int64_t clock_correction_compute_fp(struct clock_correction* corr, uint32_t last_app_time_ns, uint32_t dc_time_ns)
{
	int err;

	err = (int)(last_app_time_ns - dc_time_ns);

	/* Anti-windup */
	if (((err > 0) && (corr->out_ns < corr->limit_ns)) ||
		((err < 0) && (corr->out_ns > -corr->limit_ns))) {
		corr->isum_ns += (double)err;
	}

	corr->out_ns = (int32_t)(corr->pll_p * (double)(err) +
                             corr->pll_i * corr->isum_ns * corr->periodfp);

	return corr->out_ns;
}

int64_t clock_correction_compute(struct clock_correction *corr,
								 uint32_t last_app_time,
								 uint32_t dc_time, uint64_t period_limit)
{
	int64_t clock_offset = clock_correction_compute_fp(corr, last_app_time, dc_time);

	if (clock_offset < -(int64_t)period_limit)
		clock_offset = -(int64_t)period_limit;
	if (clock_offset > (int64_t)period_limit)
		clock_offset = (int64_t)period_limit;

	return clock_offset;
}