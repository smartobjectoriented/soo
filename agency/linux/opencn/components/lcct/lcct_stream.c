// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2020 Peter Lichard (peter.lichard@heig-vd.ch)
 */

#include "lcct_stream.h"

typedef struct {
	hal_bit_t *streamer_empty_in;
	hal_bit_t *streamer_start_in;
	hal_bit_t *streamer_stop_in;
	hal_bit_t *streamer_pause_in;

	hal_bit_t *streamer_clock_mode_out;
	hal_bit_t *streamer_enable_out;
	hal_bit_t *streamer_clock_out;

	hal_float_t *joint_pos_streamer_in[3];
} stream_data_t;

#define PIN(member) offsetof(stream_data_t, member)

static stream_data_t *data = NULL;
static double stream_init_t0 = 0;
static double offsetX = 0, offsetY = 0, offsetZ = 0;
static double stream_target_x, stream_target_y, stream_target_z;
static double stream_initial_x, stream_initial_y, stream_initial_z;
static double stream_prepare_time_x, stream_prepare_time_y, stream_prepare_time_z;

static double stream_prepare_velocity_XY = 25; /* mm/s */
static double stream_prepare_velocity_Z = 25;  /* mm/s */

static const double EXTERNAL_TRIGGER_DELAY = 0.1;

enum {
	STREAM_INIT,
	STREAM_WAIT_START,
	STREAM_START,
	STREAM_FALLING_EDGE,
	STREAM_READ_FIRST,
	STREAM_PREPARE_X,
	STREAM_PREPARE_Y,
	STREAM_PREPARE_Z,
	STREAM_WAIT_BEGIN_MACHINING,
	STREAM_RUN,
	STREAM_OUT_Z,
	STREAM_OUT_X,
	STREAM_OUT_Y,
	STREAM_PAUSED,
	STREAM_FINISHED,
	STREAM_ERR,
	STREAM_EMPTY_BUFFER,
	STREAM_DELAY_EXTERNAL_TRIGGER,
	STREAM_DELAY_SAMPLER_BEFORE_TRIGGER,
	STREAM__COUNT
};

static int stream_init(void);
static int stream_wait_start(void);
static int stream_start(void);
static int stream_falling_edge(void);
static int stream_read_first(void);
static int stream_prepare_x(void);
static int stream_prepare_y(void);
static int stream_prepare_z(void);
static int stream_wait_begin_machining(void);
static int stream_run(void);
static int stream_out_z(void);
static int stream_out_x(void);
static int stream_out_y(void);
static int stream_finished(void);
static int stream_delay_ext_trigger(void);
static int stream_delay_sampler_before_trigger(void);

DECLARE_FSM(stream, STREAM__COUNT);
static FSM(stream) fsm_stream = {
	.state = STREAM_INIT,
	.rules = {[STREAM_INIT] 		= FSM_CB(stream_init),
			  [STREAM_WAIT_START] 	= FSM_CB(stream_wait_start),
			  [STREAM_START] 		= FSM_CB(stream_start),
			  [STREAM_FALLING_EDGE] = FSM_CB(stream_falling_edge),
			  [STREAM_READ_FIRST] 	= FSM_CB(stream_read_first),
			  [STREAM_PREPARE_X] 	= FSM_CB(stream_prepare_x),
			  [STREAM_PREPARE_Y] 	= FSM_CB(stream_prepare_y),
			  [STREAM_PREPARE_Z] 	= FSM_CB(stream_prepare_z),
			  [STREAM_WAIT_BEGIN_MACHINING] = FSM_CB(stream_wait_begin_machining),
			  [STREAM_RUN] 			= FSM_CB(stream_run),
			  [STREAM_OUT_Z] 		= FSM_CB(stream_out_z),
			  [STREAM_OUT_X] 		= FSM_CB(stream_out_x),
			  [STREAM_OUT_Y] 		= FSM_CB(stream_out_y),
			  [STREAM_FINISHED] 	= FSM_CB(stream_finished),
			  [STREAM_DELAY_EXTERNAL_TRIGGER] 		= FSM_CB(stream_delay_ext_trigger),
			  [STREAM_DELAY_SAMPLER_BEFORE_TRIGGER] = FSM_CB(stream_delay_sampler_before_trigger)}};

static int stream_init(void)
{
	set_mode_csp(AXIS_X | AXIS_Y | AXIS_Z);
    set_spindle_speed(0);
	ext_trigger_enable(0);
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Please press START, STREAM_WAIT_START\n");
	return STREAM_WAIT_START;
}

static int stream_wait_start(void)
{
	if (*data->streamer_start_in) {
		if (*data->streamer_empty_in) {
			RTAPI_PRINT_MSG(RTAPI_MSG_INFO, "Please reload\n");
			return STREAM_WAIT_START;
		}
		return STREAM_START;
	}
	return STREAM_WAIT_START;
}

static int stream_start(void)
{
	/* First we need to read the first position target in the streamer and transition to it, in
	   order to prevent big jumps */
	*data->streamer_clock_mode_out = 1; // falling edge
	*data->streamer_enable_out = 1;
	*data->streamer_clock_out = 1;
	stream_init_t0 = lcct_time();
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "STREAM_FALLING_EDGE\n");

	offsetX = get_offset(AXIS_X);
	offsetY = get_offset(AXIS_Y);
	offsetZ = get_offset(AXIS_Z);

	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "STREAMING: Using offsets: x = %f, y = %f, z = %f\n", offsetX, offsetY, offsetZ);
	return STREAM_FALLING_EDGE;
}

static int stream_falling_edge(void)
{
	*data->streamer_clock_out = 0;
	if ((lcct_time() - stream_init_t0) > 0.1) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "STREAM_READ_FIRST\n");
		return STREAM_READ_FIRST;
	}
	return STREAM_FALLING_EDGE;
}

static int stream_read_first(void)
{
	stream_target_x = *data->joint_pos_streamer_in[0] + offsetX;
	stream_target_y = *data->joint_pos_streamer_in[1] + offsetY;
	stream_target_z = *data->joint_pos_streamer_in[2] + offsetZ;

    set_spindle_speed(get_external_spindle_target_speed());

	stream_initial_x = get_position(AXIS_X);
	stream_initial_y = get_position(AXIS_Y);
	stream_initial_z = get_position(AXIS_Z);

	stream_init_t0 = lcct_time();

	stream_prepare_time_x = LCCT_MAX((fabs(stream_target_x - stream_initial_x) / stream_prepare_velocity_XY), 0.1);
	stream_prepare_time_y = LCCT_MAX((fabs(stream_target_y - stream_initial_y) / stream_prepare_velocity_XY), 0.1);
	stream_prepare_time_z = LCCT_MAX((fabs(stream_target_z - stream_initial_z) / stream_prepare_velocity_Z), 0.1);

	cmd_move_axis_abs(AXIS_X, *data->joint_pos_streamer_in[AXIS_X_OFFSET] + offsetX, stream_prepare_velocity_XY);

	*data->streamer_enable_out = 0;
	*data->streamer_clock_mode_out = 0;
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "STREAM_PREPARE\n");
	return STREAM_PREPARE_X;
}

static int stream_prepare_x(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "X prepared, STREAM_PREPARE_Y\n");
		cmd_move_axis_abs(AXIS_Y, *data->joint_pos_streamer_in[AXIS_Y_OFFSET] + offsetY, stream_prepare_velocity_XY);
		return STREAM_PREPARE_Y;
	}
	return STREAM_PREPARE_X;
}

static int stream_prepare_y(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Y prepared, STREAM_PREPARE_Z\n");
		cmd_move_axis_abs(AXIS_Z, stream_target_z, stream_prepare_velocity_Z);
		return STREAM_PREPARE_Z;
	}
	return STREAM_PREPARE_Y;
}

static int stream_prepare_z(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Press START to begin machining\n");
		return STREAM_WAIT_BEGIN_MACHINING;
	}
	return STREAM_PREPARE_Z;
}

static int stream_wait_begin_machining(void)
{
    set_spindle_speed(get_external_spindle_target_speed());
	if (*data->streamer_start_in) {
		stream_init_t0 = lcct_time();
		sampler_enable(1);
		return STREAM_DELAY_SAMPLER_BEFORE_TRIGGER;
	}
	return STREAM_WAIT_BEGIN_MACHINING;
}

static int stream_delay_sampler_before_trigger(void)
{
	if (lcct_time() - stream_init_t0 > EXTERNAL_TRIGGER_DELAY) {
		ext_trigger_enable(1);
		stream_init_t0 = lcct_time();
		return STREAM_DELAY_EXTERNAL_TRIGGER;
	}
	return STREAM_DELAY_SAMPLER_BEFORE_TRIGGER;
}

static int stream_delay_ext_trigger(void)
{
	if (lcct_time() - stream_init_t0 > EXTERNAL_TRIGGER_DELAY) {
		*data->streamer_enable_out = 1;
		return STREAM_RUN;
	}
	return STREAM_DELAY_EXTERNAL_TRIGGER;
}

static int stream_run(void)
{
	set_position(AXIS_X, *data->joint_pos_streamer_in[AXIS_X_OFFSET] + offsetX);
	set_position(AXIS_Y, *data->joint_pos_streamer_in[AXIS_Y_OFFSET] + offsetY);
	set_position(AXIS_Z, *data->joint_pos_streamer_in[AXIS_Z_OFFSET] + offsetZ);

    set_spindle_speed(get_external_spindle_target_speed());

	/* if the spindle slows down too much, stop machining */
    if ((fabs(get_target_spindle_speed() - get_spindle_speed()) / (get_target_spindle_speed() + 1)) > get_spindle_threshold()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Spindle speed decreased by more than 10%%, aborting\n");
		*data->streamer_enable_out = 0;
		sampler_enable(0);

		return STREAM_OUT_Z;
	}

	if (*data->streamer_empty_in || *data->streamer_stop_in) {
		*data->streamer_enable_out = 0;
		sampler_enable(0);
        cmd_move_axis_abs(AXIS_Z, get_home_pos_z(), stream_prepare_velocity_Z);
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "streaming finished,\nspindle up, STREAM_OUT\n");
		return STREAM_OUT_Z;
	}

	return STREAM_RUN;
}

static int stream_out_z(void)
{
	if (cmd_done()) {
        cmd_move_axis_abs(AXIS_X, get_home_pos_x(), stream_prepare_velocity_XY);
		return STREAM_OUT_X;
	}
	return STREAM_OUT_Z;
}

static int stream_out_x(void)
{
	if (cmd_done()) {
        cmd_move_axis_abs(AXIS_Y, get_home_pos_y(), stream_prepare_velocity_XY);
		return STREAM_OUT_Y;
	}
	return STREAM_OUT_X;
}

static int stream_out_y(void)
{
	if (cmd_done()) {
		return STREAM_FINISHED;
	}
	return STREAM_OUT_Y;
}

static int stream_finished(void)
{
	return STREAM_INIT;
}

FSM_STATUS lcct_stream(void)
{
	FSM_UPDATE(fsm_stream);

	if (fsm_stream.state == STREAM_INIT || fsm_stream.state == STREAM_WAIT_START) {
		return FSM_FINISHED;
	} else {
		return FSM_CONTINUE;
	}
}

static const pin_def_t pin_def[] = {
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_streamer_in[0]),"lcct.stream.joint-pos-streamer-0"},
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_streamer_in[1]),"lcct.stream.joint-pos-streamer-1"},
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_streamer_in[2]),"lcct.stream.joint-pos-streamer-2"},
	{HAL_BIT,  	HAL_OUT, PIN(streamer_clock_out), 		"lcct.stream.streamer-clock"},
	{HAL_S32, 	HAL_OUT, PIN(streamer_clock_mode_out), 	"lcct.stream.streamer-clock-mode"},
	{HAL_BIT, 	HAL_IN,  PIN(streamer_start_in), 		"lcct.stream.start"},
	{HAL_BIT, 	HAL_IN,  PIN(streamer_stop_in),			"lcct.stream.stop"},
	{HAL_BIT, 	HAL_IN,  PIN(streamer_pause_in),		"lcct.stream.pause"},
	{HAL_BIT, 	HAL_OUT, PIN(streamer_enable_out),		"lcct.stream.streamer-enable"},
	{HAL_BIT, 	HAL_IN,  PIN(streamer_empty_in), 		"lcct.stream.streamer-empty"},
	{HAL_TYPE_UNSPECIFIED}
};

int lcct_stream_init(int comp_id)
{
	HAL_INIT_PINS(pin_def, comp_id, data);

	add_hal_button(data->streamer_start_in);
	add_hal_button(data->streamer_stop_in);
	add_hal_button(data->streamer_pause_in);
	return 0;

}

void lcct_stream_reset(void) { fsm_stream.state = STREAM_INIT; }

void lcct_stream_stop(void) { *data->streamer_enable_out = 0; }
