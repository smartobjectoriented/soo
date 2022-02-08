/********************************************************************
 *  Copyright (C) 2019 Peter Lichard <peter.lichard@heig-vd.ch>
 *  Copyright (C) 2019 Jean-Pierre Miceli Miceli <jean-pierre.miceli@heig-vd.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 ********************************************************************/

#ifndef FEEDOPT_PRIV_H
#define FEEDOPT_PRIV_H

#include <opencn/uapi/feedopt.h>

typedef enum {
	FEEDOPT_STATE_INACTIVE,
    FEEDOPT_STATE_RUNNING,
} FEEDOPT_STATE;

typedef struct {
	int head, tail;
	feedopt_sample_t *feedopt_data;
	int capacity;
	volatile int size;
} fopt_rg_t;


/* this structure contains the HAL shared memory feedopt_data for the component
 */
typedef struct {
	hal_float_t *const pin_sample_pos_out[3]; /* pins: resampled x,y,z positions */
    hal_float_t *const spindle_speed_out;
	hal_bit_t *const rt_active, *const rt_single_shot, *const us_active;
    hal_bit_t *const ready_out, *const finished_out, *const underrun_out;
	hal_u32_t *const pin_buffer_underrun_count;
	hal_bit_t *const opt_rt_reset, *const commit_cfg, *const opt_us_reset;
	hal_u32_t *const queue_size;
	hal_float_t *const opt_per_second;
	hal_float_t *const current_u;
	hal_float_t *const manual_override, *const auto_override;
	hal_bit_t *const rt_start, *const us_start;
	hal_bit_t *const rt_moving;
	hal_bit_t *const rt_has_sample;
	hal_u32_t *const sampling_period_ns;

	hal_s32_t *const us_optimising_count;
	hal_s32_t *const us_optimising_progress;
	hal_s32_t *const rt_resampling_progress;
	hal_bit_t *const us_resampling_paused;
    hal_s32_t *const current_gcode_line;
} feedopt_hal_t;

void feedopt_reset(void);

#endif /* FEEDOPT_PRIV_H */


