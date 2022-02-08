// SPDX-License-Identifier: GPL-2.0-only

/********************************************************************
 * Description:  feedopt.c
 *               A HAL component that can be used to capture feedopt_data
 *               from HAL pins at a specific realtime sample rate,
 *		and allows the feedopt_data to be written to stdout.
 *
 * Author: John Kasunich <jmkasunich at sourceforge dot net>
 * License: GPL Version 2
 *
 * Copyright (c) 2006 All rights reserved.
 *
 ********************************************************************/
/** This file, 'feedopt_ft.c', is the realtime with floating support part of feedopt.
 *
 */
#include <linux/types.h>

#include <opencn/ctypes/strings.h>

#include <opencn/uapi/feedopt.h>

#include <opencn/components/feedopt.h>

typedef enum {
	SAMPLE_OK = 0,
    SAMPLE_END = 1,
    SAMPLE_UNDERRUN = 2,
} SAMPLE_STATE;

static int buffer_underrun = 0;

extern fopt_rg_t samples_queue;
extern feedopt_sample_t current_sample;

extern feedopt_hal_t *fopt_hal;

extern FEEDOPT_STATE state;

static int fopt_rg_pop(fopt_rg_t *rg, feedopt_sample_t *value)
{
	if (rg->size > 0) {
		*value = rg->feedopt_data[rg->tail++];
		__sync_fetch_and_add(&rg->size, -1);
		if (rg->tail == rg->capacity)
			rg->tail = 0;
		return 1;
	}
	return 0;
}

static SAMPLE_STATE pop_next_sample(void)
{
	int pop_result = fopt_rg_pop(&samples_queue, &current_sample);
	if (pop_result) {
        if (current_sample.end_flag) {
            RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "[FEEDOPT] Received sample end_flag\n");
            return SAMPLE_END;
		}
		else {
			buffer_underrun = 0;
			return SAMPLE_OK;
		}
	}
	else {
		if (!buffer_underrun) {
			buffer_underrun = 1;
			opencn_cprintf(OPENCN_COLOR_BRED, "[FEEDOPT] BUFFER UNDERRUN!\n");
		}
		*fopt_hal->pin_buffer_underrun_count += 1;
        return SAMPLE_UNDERRUN;
	}
}

static void feedopt_resample(void)
{
    const int result = pop_next_sample();
    if (result == SAMPLE_END) {
        *fopt_hal->finished_out = 1;
        state = FEEDOPT_STATE_INACTIVE;
    }
    else if (result == SAMPLE_UNDERRUN) {
        *fopt_hal->underrun_out = 1;
		state = FEEDOPT_STATE_INACTIVE;
	}
    else {
        *fopt_hal->pin_sample_pos_out[0] = current_sample.axis_position[0];
        *fopt_hal->pin_sample_pos_out[1] = current_sample.axis_position[1];
        *fopt_hal->pin_sample_pos_out[2] = current_sample.axis_position[2];

        *fopt_hal->rt_resampling_progress = current_sample.index;
        *fopt_hal->current_gcode_line = current_sample.gcode_line;
        *fopt_hal->spindle_speed_out = current_sample.spindle_speed;
    }
}


static void feedopt_state_inactive(void)
{
	*fopt_hal->rt_active = 0;

	if (*fopt_hal->rt_start) {
		state = FEEDOPT_STATE_RUNNING;
        *fopt_hal->underrun_out = 0;
        *fopt_hal->finished_out = 0;
        RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "FEEDOPT(inactive) received start\n");
	}

	else if (*fopt_hal->opt_rt_reset) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "FEEDOPT(inactive) received reset\n");
		feedopt_reset();
	}

	else if (*fopt_hal->rt_single_shot) {
		feedopt_resample();
	}
}

static void feedopt_state_running(double DT0)
{
	*fopt_hal->rt_active = 1;
	if (*fopt_hal->opt_rt_reset) {
        state = FEEDOPT_STATE_INACTIVE;
        feedopt_reset();
	}
    else {
        feedopt_resample();
    }
}


void feedopt_update_fp(FEEDOPT_STATE state, long period)
{
	const double DT0 = period * 1e-9;

	switch (state) {
	case FEEDOPT_STATE_INACTIVE:
		feedopt_state_inactive();
		break;
	case FEEDOPT_STATE_RUNNING:
		feedopt_state_running(DT0);
		break;
    }

	*fopt_hal->queue_size = samples_queue.size;
    *fopt_hal->ready_out = samples_queue.size > 0;

	*fopt_hal->rt_start = 0;
	*fopt_hal->opt_rt_reset = 0;
	*fopt_hal->rt_single_shot = 0;
}

