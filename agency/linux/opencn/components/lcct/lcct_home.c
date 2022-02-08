// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2020 Peter Lichard (peter.lichard@heig-vd.ch)
 */

#include "lcct_internal.h"

typedef struct {
	hal_bit_t *start_homing_sequence_in, *stop_homing_sequence_in;
	hal_bit_t *start_homing_out[4];
	hal_bit_t *stop_homing_out[4];
	hal_bit_t *homed_in[4];
} home_data_t;

#define PIN(member) offsetof(home_data_t, member)

static home_data_t *home_data = NULL;
static int delay = 0;

enum { HOME_INIT, HOME_WAIT_START, HOME_START, HOME_AXIS_0, HOME_AXIS_1, HOME_AXIS_2, HOME__COUNT };

int home_init(void);
int home_wait_start(void);
int home_start(void);
int home_axis_0(void);
int home_axis_1(void);
int home_axis_2(void);

DECLARE_FSM(home, HOME__COUNT);
static FSM(home) fsm_home = {.state = HOME_INIT,
			     .rules = {[HOME_INIT] = FSM_CB(home_init),
				       [HOME_WAIT_START] 	= FSM_CB(home_wait_start),
				       [HOME_START] 		= FSM_CB(home_start),
				       [HOME_AXIS_0] 		= FSM_CB(home_axis_0),
				       [HOME_AXIS_1] 		= FSM_CB(home_axis_1),
				       [HOME_AXIS_2] 		= FSM_CB(home_axis_2)}};

static const pin_def_t pin_def[] = {
	{HAL_BIT, HAL_IN,  PIN(start_homing_sequence_in), "lcct.home.start-homing-sequence"},
	{HAL_BIT, HAL_IN,  PIN(stop_homing_sequence_in), "lcct.home.stop-homing-sequence"},
	{HAL_BIT, HAL_OUT, PIN(start_homing_out[0]), "lcct.home.start-homing-0"},
	{HAL_BIT, HAL_OUT, PIN(start_homing_out[1]), "lcct.home.start-homing-1"},
	{HAL_BIT, HAL_OUT, PIN(start_homing_out[2]), "lcct.home.start-homing-2"},
	{HAL_BIT, HAL_OUT, PIN(start_homing_out[3]), "lcct.home.start-homing-3"},
	{HAL_BIT, HAL_OUT, PIN(stop_homing_out[0]), "lcct.home.stop-homing-0"},
	{HAL_BIT, HAL_OUT, PIN(stop_homing_out[1]), "lcct.home.stop-homing-1"},
	{HAL_BIT, HAL_OUT, PIN(stop_homing_out[2]), "lcct.home.stop-homing-2"},
	{HAL_BIT, HAL_OUT, PIN(stop_homing_out[3]), "lcct.home.stop-homing-3"},
	{HAL_BIT, HAL_IN,  PIN(homed_in[0]), "lcct.home.homed-0"},
	{HAL_BIT, HAL_IN,  PIN(homed_in[1]), "lcct.home.homed-1"},
	{HAL_BIT, HAL_IN,  PIN(homed_in[2]), "lcct.home.homed-2"},
	{HAL_BIT, HAL_IN,  PIN(homed_in[3]), "lcct.home.homed-3"},


	{HAL_TYPE_UNSPECIFIED}
};

int lcct_home_init(int comp_id)
{
	HAL_INIT_PINS(pin_def, comp_id, home_data);

	add_hal_button(home_data->start_homing_sequence_in);
	add_hal_button(home_data->stop_homing_sequence_in);

	return 0;
}

FSM_STATUS lcct_home(void)
{
	if (*home_data->stop_homing_sequence_in) {
		*home_data->stop_homing_out[AXIS_X_OFFSET] = 1;
		*home_data->stop_homing_out[AXIS_Y_OFFSET] = 1;
		*home_data->stop_homing_out[AXIS_Z_OFFSET] = 1;
		fsm_home.state = HOME_INIT;
	}

	FSM_UPDATE(fsm_home);
	delay++;
	if (fsm_home.state == HOME_WAIT_START) {
		return FSM_FINISHED;
	} else {
		return FSM_CONTINUE;
	}
}

int home_init(void)
{
	set_mode_hm(AXIS_X | AXIS_Y | AXIS_Z);
	set_mode_inactive(AXIS_W);

	*home_data->stop_homing_out[0] = 1;
	*home_data->stop_homing_out[1] = 1;
	*home_data->stop_homing_out[2] = 1;

	*home_data->start_homing_out[0] = 0;
	*home_data->start_homing_out[1] = 0;
	*home_data->start_homing_out[2] = 0;

	return HOME_WAIT_START;
}

int home_wait_start(void)
{
	if (*home_data->start_homing_sequence_in) {
		return HOME_START;
	}
	return HOME_WAIT_START;
}

int home_start(void)
{
	*home_data->stop_homing_out[AXIS_X_OFFSET] = 0;
	*home_data->start_homing_out[AXIS_X_OFFSET] = 1;
	delay = 0;
	return HOME_AXIS_0;
}

int home_axis_0(void)
{
	if (*home_data->homed_in[AXIS_X_OFFSET] && delay > 10000) {
		*home_data->stop_homing_out[AXIS_Y_OFFSET] = 0;
		*home_data->start_homing_out[AXIS_Y_OFFSET] = 1;
		delay = 0;
		return HOME_AXIS_1;
	}
	return HOME_AXIS_0;
}

int home_axis_1(void)
{
	if (*home_data->homed_in[AXIS_Y_OFFSET] && delay > 10000) {
		*home_data->stop_homing_out[AXIS_Z_OFFSET] = 0;
		*home_data->start_homing_out[AXIS_Z_OFFSET] = 1;
		delay = 0;
		return HOME_AXIS_2;
	}
	return HOME_AXIS_1;
}

int home_axis_2(void)
{
	if (*home_data->homed_in[AXIS_Z_OFFSET] && delay > 10000) {
		delay = 0;
		return HOME_INIT;
	}
	return HOME_AXIS_2;
}

void lcct_home_reset(void) { fsm_home.state = HOME_INIT; }

int lcct_is_homed(void)
{
	return *home_data->homed_in[AXIS_X_OFFSET] && *home_data->homed_in[AXIS_Y_OFFSET] && *home_data->homed_in[AXIS_Z_OFFSET];
}
