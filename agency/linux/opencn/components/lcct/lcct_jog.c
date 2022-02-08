// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2020 Peter Lichard (peter.lichard@heig-vd.ch)
 */

#include "lcct_jog.h"

typedef struct {
	hal_float_t *move_rel_in;
	hal_float_t *move_abs_in;
	hal_float_t *velocity_in;
	hal_bit_t *plus_button_in;
	hal_bit_t *minus_button_in;
	hal_bit_t *go_to_button_in;
	hal_bit_t *stop_button_in;
	hal_bit_t *axis_X_in, *axis_Y_in, *axis_Z_in;
} jog_data_t;

#define PIN(member) offsetof(jog_data_t, member)

static jog_data_t *data = NULL;
static int is_relative = 0;

enum { JOG_INIT, JOG_WAIT_EVENT, JOG_RUN, JOG__COUNT };

static int jog_init(void);
static int jog_wait_event(void);
static int jog_run(void);



DECLARE_FSM(jog, JOG__COUNT);
static FSM(jog) fsm_jog = {
	.state = JOG_INIT,
	.rules = {[JOG_INIT] = FSM_CB(jog_init), [JOG_WAIT_EVENT] = FSM_CB(jog_wait_event), [JOG_RUN] = FSM_CB(jog_run)}};

static int jog_init(void)
{
	set_mode_csp(AXIS_X | AXIS_Y | AXIS_Z);
    set_spindle_speed(get_external_spindle_target_speed());
	return JOG_WAIT_EVENT;
}

static int jog_wait_event(void)
{
	AXES_ENUM axis = AXIS_NONE;
    set_spindle_speed(get_external_spindle_target_speed());

	if (*data->axis_X_in)
		axis = AXIS_X;
	if (*data->axis_Y_in)
		axis = AXIS_Y;
	if (*data->axis_Z_in)
		axis = AXIS_Z;

	if (axis == AXIS_NONE) {
		return JOG_WAIT_EVENT;
	}

	if (*data->plus_button_in) {
		cmd_move_axis_rel(axis, *data->move_rel_in, *data->velocity_in / 60.0f);
		is_relative = 1;
		return JOG_RUN;
	} else if (*data->minus_button_in) {
		cmd_move_axis_rel(axis, -*data->move_rel_in, *data->velocity_in / 60.0f);
		is_relative = 1;
		return JOG_RUN;
	} else if (*data->go_to_button_in) {
		cmd_move_axis_abs(axis, *data->move_abs_in, *data->velocity_in / 60.0f);
		is_relative = 0;
		return JOG_RUN;
	}

	return JOG_WAIT_EVENT;
}

static int jog_run(void)
{
	if (cmd_done()) {
		return JOG_WAIT_EVENT;
	}
	return JOG_RUN;
}

FSM_STATUS lcct_jog(void)
{
	if (*data->stop_button_in) {
		fsm_jog.state = JOG_INIT;
        cmd_stop();
	}

	FSM_UPDATE(fsm_jog);
	if (fsm_jog.state == JOG_RUN) {
		return FSM_CONTINUE;
	} else {
		return FSM_FINISHED;
	}
}

static const pin_def_t pin_def[] = {
	{HAL_FLOAT, HAL_IN, PIN(velocity_in),		"lcct.jog.velocity"},
	{HAL_FLOAT, HAL_IN, PIN(move_abs_in), 		"lcct.jog.move-abs"},
	{HAL_FLOAT, HAL_IN, PIN(move_rel_in), 		"lcct.jog.move-rel"},
	{HAL_BIT, 	HAL_IN, PIN(plus_button_in), 	"lcct.jog.plus"},
	{HAL_BIT, 	HAL_IN, PIN(minus_button_in),	"lcct.jog.minus"},
	{HAL_BIT, 	HAL_IN, PIN(go_to_button_in),	"lcct.jog.goto"},
	{HAL_BIT, 	HAL_IN, PIN(stop_button_in), 	"lcct.jog.stop"},
	{HAL_BIT, 	HAL_IN, PIN(axis_X_in), 		"lcct.jog.axis-X"},
	{HAL_BIT, 	HAL_IN, PIN(axis_Y_in), 		"lcct.jog.axis-Y"},
	{HAL_BIT, 	HAL_IN, PIN(axis_Z_in), 		"lcct.jog.axis-Z"},


	{HAL_TYPE_UNSPECIFIED}
};

int lcct_jog_init(int comp_id)
{
	HAL_INIT_PINS(pin_def, comp_id, data);

	add_hal_button(data->plus_button_in);
	add_hal_button(data->minus_button_in);
	add_hal_button(data->go_to_button_in);
	add_hal_button(data->stop_button_in);

	return 0;
}

int lcct_jog_is_relative(void) {
	return is_relative;
}

void lcct_jog_reset(void) { fsm_jog.state = JOG_INIT; }
