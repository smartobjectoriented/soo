/********************************************************************
 *  Copyright (C) 2019  Peter Lichard  <peter.lichard@heig-vd.ch>
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

/**
  * @file lcct_rt.c
  */

#include <soo/uapi/console.h>

#include "lcct_home.h"
#include "lcct_gcode.h"
#include "lcct_jog.h"
#include "lcct_stream.h"
#include "lcct_internal.h"

typedef struct interp {
	double x0, x1;
	double t0;
	double linear_speed;
	double T;
} interp_t;

typedef enum {
	CMD_NONE,
	CMD_MOVE_AXIS, // single axis move
} COMMAND;

typedef struct {
	double min;
	double max;
} limits_t;

static struct {
	COMMAND type;
	int finished;
	union {
		struct {
			interp_t interp;
			AXES_ENUM axis;
		} move_axis;
	};
} _current_command;

#define DECIMATE(cmd, rate) \
{\
    static int _decimate_counter = 0; \
    if (++_decimate_counter >= rate) {  \
        cmd;                          \
        _decimate_counter = 0;        \
    }                                 \
}

static const char* AXIS_NAME[AXIS__COUNT] = {
	"X",
	"Y",
	"Z",
	"Spindle"
};

static  MAIN_STATE main_state;

static int homing_done = 0;
static double desired_target_position[4] = {0};

static double cur_time = 0; // in seconds
static double dt = 0;

static double target_spindle_vel = 0;
static double desired_spindle_vel = 0; /* In turns per second */
static int spindle_was_active = 0;
static double spindle_start_time = 0.0;

const limits_t axis_limit[AXIS__COUNT] = {
		[AXIS_X_OFFSET] = {.min = -24, .max = 24},
		[AXIS_Y_OFFSET] = {.min = -24, .max = 24},
		[AXIS_Z_OFFSET] = {.min = -20, .max = 25},
		[AXIS_W_OFFSET] = {.min = 0, .max = 0}};

static const int AXES[] = {AXIS_X_OFFSET, AXIS_Y_OFFSET, AXIS_Z_OFFSET};

double lcct_time(void) { return cur_time; }


extern lcct_data_t *lcct_data;
extern hal_bit_t *hal_buttons[1024];
extern int hal_button_count;

transform_t unit_transform(void) {
	transform_t tform = {.x = 0.0, .y = 0.0, .z = 0.0, .rz = 0.0};
	return tform;
}

void reset_hal_buttons(void)
{
	int i;
	for (i = 0; i < hal_button_count; i++) {
		*hal_buttons[i] = 0;
	}
}

static bool interp_get(interp_t *ip, double *output)
{
	double t = (cur_time - ip->t0) / ip->T; /* normalized time */
    if (t > 1.0) {
        *output = ip->x1;
        return false;
    } else {
        *output = ip->x0 + (1.0 - cos(t * M_PI)) * 0.5 * (ip->x1 - ip->x0);
        return true;
    }
}


static void cmd_exec(void)
{
	switch (_current_command.type) {
	case CMD_NONE:
		_current_command.finished = 1;
		break;
	case CMD_MOVE_AXIS: {
		double target = 0;
		if (!interp_get(&_current_command.move_axis.interp, &target)) {
			_current_command.finished = 1;
			_current_command.type = CMD_NONE;
		}
		set_position(_current_command.move_axis.axis, target);
	} break;
	}
}


void main_state_init(void) { main_state = MAIN_IDLE; }

static int set_target_pos_xyz(void)
{
	int i = 0;

	if (main_state == MAIN_GCODE || main_state == MAIN_STREAM) {
		for (i = 0; i < 3; ++i) {
			const int off = AXES[i];
			if (desired_target_position[off] < axis_limit[off].min) {
				opencn_printf("Warning: Axis %s limit exceeded, %.2f < %.2f\n",
							  AXIS_NAME[off], desired_target_position[off], axis_limit[off].min);
				return 1;
			}
			if (desired_target_position[off] > axis_limit[off].max) {
				opencn_printf("Warning: Axis %s limit exceeded, %.2f > %.2f\n",
							  AXIS_NAME[off], desired_target_position[off], axis_limit[off].max);
				return 1;
			}
		}
	}

	switch (main_state) {
	case MAIN_HOMING:

		break;

	case MAIN_GCODE:
	case MAIN_STREAM:
	case MAIN_JOG:
		*lcct_data->target_position_out[AXIS_X_OFFSET] = desired_target_position[AXIS_X_OFFSET];
		*lcct_data->target_position_out[AXIS_Y_OFFSET] = desired_target_position[AXIS_Y_OFFSET];
		*lcct_data->target_position_out[AXIS_Z_OFFSET] = desired_target_position[AXIS_Z_OFFSET];

		break;
	case MAIN_IDLE:
		break;
	case MAIN_ERR:
		set_mode_inactive(AXIS_ALL);
		break;
	}

	return 0;
}

static int cmd_check_done(void)
{
	if (!_current_command.finished) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "COMMAND not finished yet!\n");
		_current_command.type = CMD_NONE;
		main_state = MAIN_ERR;
		return 0;
	}
	return 1;
}

static void interp_init(interp_t *ip, double x0, double x1, double speed)
{
	ip->x0 = x0;
	ip->x1 = x1;
	ip->t0 = cur_time;
	ip->linear_speed = speed;
	ip->T = LCCT_MAX((fabs(x1 - x0) / speed), 0.1);
}

int axis_offset(AXES_ENUM axis)
{
	switch (axis) {
	case AXIS_X:
		return AXIS_X_OFFSET;
	case AXIS_Y:
		return AXIS_Y_OFFSET;
	case AXIS_Z:
		return AXIS_Z_OFFSET;
	case AXIS_W:
		return AXIS_W_OFFSET;
	default:
		lprintk("LCCT: Wrong axis ID\n");
		BUG_ON(1);
	}
}

double get_position(AXES_ENUM axis) { return *lcct_data->joint_pos_cur_in[axis_offset(axis)]; }
double get_offset(AXES_ENUM axis) { return *lcct_data->spinbox_offset_in[axis_offset(axis)]; }
double get_offset_thetaZ() {return *lcct_data->spinbox_offset_thetaZ;}
void set_position(AXES_ENUM axis, double value) { desired_target_position[axis_offset(axis)] = value; }
void ext_trigger_enable(int b) { *lcct_data->external_trigger_out = b; }

double get_target_spindle_speed(void) { return *lcct_data->spindle_cmd_out; }

double get_spindle_speed(void) { return *lcct_data->spindle_cur_in; }

double get_external_spindle_target_speed(void)
{
    return *lcct_data->gui_spindle_target_velocity;
}

void sampler_enable(int b) {
    if (*lcct_data->sampler_enable_out != b) {
        /* lprintk("sampler_enable(%d)\n", b); */
    }
    *lcct_data->sampler_enable_out = b;
}

void cmd_move_axis_rel(AXES_ENUM axis, double offset, double speed)
{
	if (!cmd_check_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "cmd_move_axis_rel: Command not done, refusing to issue a new one\n");
		return;
	}
	interp_init(&_current_command.move_axis.interp, get_position(axis), get_position(axis) + offset, speed);
	_current_command.type = CMD_MOVE_AXIS;
	_current_command.move_axis.axis = axis;
	_current_command.finished = 0;
}

void cmd_move_axis_abs(AXES_ENUM axis, double target_position, double speed)
{
	if (!cmd_check_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "cmd_move_axis_abs: !!! Command not done, refusing to issue a new one !!!\n");
		return;
	} else if (!homing_done) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Please do homing first\n");
		return;
	}
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "cmd_move_axis_abs: axis = %d, target_position = %f, speed = %f\n", axis, target_position, speed);
	interp_init(&_current_command.move_axis.interp, get_position(axis), target_position, speed);
	_current_command.type = CMD_MOVE_AXIS;
	_current_command.move_axis.axis = axis;
	_current_command.finished = 0;
}

void cmd_stop(void) {
    _current_command.finished = 1;
    _current_command.type = CMD_NONE;
}

void set_spindle_speed(double speed)
{
    target_spindle_vel = speed;
}

void set_mode_csp_axis(AXES_ENUM axis)
{
	int k = axis_offset(axis);
	if (*lcct_data->in_mode_csp_in[k] == 0) {
		/* prepare the target position to be the current position
		   before going into CSP mode */
		*lcct_data->target_position_out[k] = get_position(axis);
		*lcct_data->set_mode_csp_out[k] = 1;
	}
	desired_target_position[k] = get_position(axis);
}

void set_mode_csp(int flags)
{
	if (flags & AXIS_X)
		set_mode_csp_axis(AXIS_X);
	if (flags & AXIS_Y)
		set_mode_csp_axis(AXIS_Y);
	if (flags & AXIS_Z)
		set_mode_csp_axis(AXIS_Z);
	if (flags & AXIS_W)
		set_mode_csp_axis(AXIS_W);
}

void set_mode_csv_axis(AXES_ENUM axis)
{
	int k = axis_offset(axis);
	if (*lcct_data->in_mode_csv_in[k] == 0) {
		*lcct_data->set_mode_csv_out[k] = 1;
	}
}

void set_mode_csv(int flags)
{
	if (flags & AXIS_X)
		set_mode_csv_axis(AXIS_X);
	if (flags & AXIS_Y)
		set_mode_csv_axis(AXIS_Y);
	if (flags & AXIS_Z)
		set_mode_csv_axis(AXIS_Z);
	if (flags & AXIS_W)
		set_mode_csv_axis(AXIS_W);
}

void set_mode_hm_axis(AXES_ENUM axis)
{
	int k = axis_offset(axis);
	if (*lcct_data->in_mode_hm_in[k] == 0) {
		*lcct_data->set_mode_hm_out[k] = 1;
	}
}

double get_spindle_threshold(void)
{
    return *lcct_data->spindle_threshold;
}

double get_spindle_acceleration(void)
{
    /* spindle_acceleration is in RPM/s and we want turns/s/s */
    return *lcct_data->spindle_acceleration / 60.0;
}

void set_mode_hm(int flags)
{
	if (flags & AXIS_X)
		set_mode_hm_axis(AXIS_X);
	if (flags & AXIS_Y)
		set_mode_hm_axis(AXIS_Y);
	if (flags & AXIS_Z)
		set_mode_hm_axis(AXIS_Z);
	if (flags & AXIS_W)
		set_mode_hm_axis(AXIS_W);
}

void set_mode_inactive_axis(AXES_ENUM axis)
{
	int k = axis_offset(axis);
	if (*lcct_data->in_mode_inactive_in[k] == 0) {
		*lcct_data->set_mode_inactive_out[k] = 1;
	}
}

void set_mode_inactive(int flags)
{
	if (flags & AXIS_X)
		set_mode_inactive_axis(AXIS_X);
	if (flags & AXIS_Y)
		set_mode_inactive_axis(AXIS_Y);
	if (flags & AXIS_Z)
		set_mode_inactive_axis(AXIS_Z);
	if (flags & AXIS_W)
		set_mode_inactive_axis(AXIS_W);
}

double get_home_pos_x(void) {
	return *lcct_data->home_position_in[AXIS_X_OFFSET];
}

double get_home_pos_y(void) {
	return *lcct_data->home_position_in[AXIS_Y_OFFSET];
}

double get_home_pos_z(void) {
    return *lcct_data->home_position_in[AXIS_Z_OFFSET];
}

int cmd_done(void) { return _current_command.finished; }

void set_command_finished(void) {  _current_command.finished = 1; }

/**
 * @brief       HAL-exported function acting as the realtime-callback
 * \callgraph
 */
void lcct_update_rt(void *arg, long period)
{
	int axis_i;
	bool was_running;
	dt = period / 1e9;
	cur_time = cur_time + dt; // update current time in second

	homing_done = lcct_is_homed();

	/* Start by checking if any axis is in a fault state,
	   if it is, we want to go be inactive immediately */
	if (*lcct_data->in_fault_in[0] || *lcct_data->in_fault_in[1] || *lcct_data->in_fault_in[2] || *lcct_data->in_fault_in[3]) {
		send_reset();
		lcct_stream_stop();
		main_state = MAIN_IDLE;
	}

	if (*lcct_data->fault_reset_in) {
        *lcct_data->fault_reset_in = 0;
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Resetting drive faults\n");
		for (axis_i = 0; axis_i < AXIS__COUNT; axis_i++) {
			*lcct_data->fault_reset_out[axis_i] = 1;
		}
	}

	if (*lcct_data->set_machine_mode_homing_in) {
        *lcct_data->set_machine_mode_homing_in = 0;
		main_state = MAIN_HOMING;
		lcct_home_reset();
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Mode set to MAIN_HOMING\n");
	} else if (*lcct_data->set_machine_mode_gcode_in) {
        *lcct_data->set_machine_mode_gcode_in = 0;
		if (!homing_done) {
			RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Please do homing first\n");
		} else {
			main_state = MAIN_GCODE;
			lcct_gcode_reset();
			RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Mode set to MAIN_GCODE\n");
		}
	} else if (*lcct_data->set_machine_mode_stream_in) {
        *lcct_data->set_machine_mode_stream_in = 0;
		if (!homing_done) {
			RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Please do homing first\n");
		} else {
			main_state = MAIN_STREAM;
			lcct_stream_reset();
			RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Mode set to MAIN_STREAM\n");
		}
	} else if (*lcct_data->set_machine_mode_jog_in) {
        *lcct_data->set_machine_mode_jog_in = 0;
		main_state = MAIN_JOG;
		lcct_jog_reset();
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Mode set to MAIN_JOG\n");
	} else if (*lcct_data->set_machine_mode_inactive_in) {
        *lcct_data->set_machine_mode_inactive_in = 0;
		main_state = MAIN_IDLE;
		set_mode_inactive(AXIS_ALL);
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "Mode set to MAIN_IDLE\n");
	}

	cmd_exec();
	if (set_target_pos_xyz()) {
		main_state = MAIN_ERR;
	}

	*lcct_data->in_machine_mode_gcode_out = main_state == MAIN_GCODE;
	*lcct_data->in_machine_mode_stream_out = main_state == MAIN_STREAM;
	*lcct_data->in_machine_mode_jog_out = main_state == MAIN_JOG;
	*lcct_data->in_machine_mode_homing_out = main_state == MAIN_HOMING;
	*lcct_data->in_machine_mode_inactive_out = main_state == MAIN_IDLE;

	*lcct_data->disable_abs_jog = !homing_done;

	switch (main_state) {
	case MAIN_HOMING:
	    sampler_enable(0);
		*lcct_data->homing_finished_out = lcct_home();
		break;
	case MAIN_STREAM:
		was_running = *lcct_data->stream_running_out;
		*lcct_data->stream_running_out = lcct_stream() == FSM_CONTINUE;
		if (was_running && !*lcct_data->stream_running_out) {
			*lcct_data->stream_finished_out = true;
		}
		break;
	case MAIN_GCODE:
		was_running = *lcct_data->gcode_running_out;
		*lcct_data->gcode_running_out = lcct_gcode() == FSM_CONTINUE;
		if (was_running && !*lcct_data->gcode_running_out) {
			*lcct_data->gcode_finished_out = true;
		}
		break;
	case MAIN_JOG:
        sampler_enable(0);
		*lcct_data->jog_finished_out = lcct_jog();
		break;
	case MAIN_IDLE:
	case MAIN_ERR:
	    sampler_enable(0);
		break;
	default:
		lprintk("LCCT: INVALID STATE\n");
		BUG();
	}

	if (*lcct_data->spindle_active) {
		if (!spindle_was_active) {
			*lcct_data->set_mode_csv_out[3] = 1;
			spindle_was_active = 1;
			spindle_start_time = cur_time;
			*lcct_data->spindle_cmd_out = 0;
		}
        desired_spindle_vel = target_spindle_vel / 60.0f;
	} else {
		if (spindle_was_active) {
			*lcct_data->set_mode_inactive_out[3] = 1;
			spindle_was_active = 0;
			*lcct_data->spindle_cmd_out = 0;
		}
		desired_spindle_vel = 0;
	}

    if (fabs(desired_spindle_vel - *lcct_data->spindle_cmd_out) > get_spindle_acceleration() * dt) {
		if (*lcct_data->in_mode_csv_in[3] && (cur_time - spindle_start_time > 2.0)) {
            *lcct_data->spindle_cmd_out += SIGN(desired_spindle_vel - *lcct_data->spindle_cmd_out) * get_spindle_acceleration() * dt;
		}
	} else {
		*lcct_data->spindle_cmd_out = desired_spindle_vel;
	}

	if (fabs(*lcct_data->spindle_cur_in) > 1) {
		*lcct_data->electrovalve_out = 1;
	} else {
		*lcct_data->electrovalve_out = 0;
	}

	*lcct_data->spindle_cur_out = *lcct_data->spindle_cur_in * 60;
	if (*lcct_data->fault_reset_in) {
		set_mode_inactive(AXIS_ALL);
		main_state = MAIN_IDLE;
	}

	*lcct_data->homed_out = homing_done;
	reset_hal_buttons();
}












