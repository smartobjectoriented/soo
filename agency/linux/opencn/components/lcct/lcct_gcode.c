// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2020 Peter Lichard (peter.lichard@heig-vd.ch)
 */

/**
 * @file lcct_gcode.c
 * @brief Implements the lcct_gcode submodule
 */

#include <linux/time32.h>


#include "lcct_gcode.h"


/**
 * @def gcode_data_t
 * @brief Hal pins
 */
typedef struct {
    hal_bit_t *const start_in;				/**< Button-like pin for starting the machining				*/
    hal_bit_t *const feedopt_single_shot_out; /**< Output to feedopt for requesting a single resample	*/
    hal_bit_t *const feedopt_us_active_in;	/**< Input from feedopt about optimization status			*/
    hal_bit_t *const feedopt_us_start_out;	/**< Output to feedopt for starting optimization			*/
    hal_bit_t *const feedopt_rt_active_in;	/**< Input from feedopt about the resample state			*/
    hal_bit_t *const feedopt_rt_start_out;	/**< Output to feedopt for starting the resampling			*/
    hal_bit_t *const pause_in;     /**< Input from GUI for pausing */
    hal_bit_t *const feedopt_rt_pause_out; /**< Output to feedopt for pausing */
    hal_bit_t *const feedopt_ready_in; /**< Input from feedopt about its readiness (is the queue at least
                                 one deep?) */
    hal_bit_t *const feedopt_finished_in; /**< Did feedopt finish pushing resampled data */
    hal_bit_t *const feedopt_underrun_in; /**< Did feedopt underrun */

    hal_bit_t *const feedopt_reset_in;
    hal_bit_t *const feedopt_rt_reset_out;  /**< Output to feedopt resampler for requesting a full reset	*/
    hal_bit_t *const feedopt_us_reset_out; /**< Output to feedopt optimizer for requesting a full reset		*/
    hal_float_t *const joint_pos_cmd_in[4]; /**< Input from feedopt resampler about the target position		*/
    hal_bit_t *const feedopt_rt_has_segment; /**< Input from feedopt, true if it has a segment to resample	*/
    hal_s32_t *const lcct_gcode_runtime_s;

    hal_bit_t *const lcct_gcode_done;
    hal_float_t *const feedopt_spindle_speed;
} gcode_data_t;

#define PIN(member) offsetof(gcode_data_t, member)

static gcode_data_t *data = NULL;
static double offsetX = 0, offsetY = 0, offsetZ = 0, offsetThetaZ;

#if !defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
static double offsetCosTheta = 0, offsetSinTheta = 0;
#endif

static double gcode_init_t0 = 0;
static double gcode_target_x, gcode_target_y, gcode_target_z;
static double gcode_initial_x, gcode_initial_y, gcode_initial_z;
static double gcode_prepare_time_x, gcode_prepare_time_y, gcode_prepare_time_z;

static const double gcode_prepare_velocity_XY = 25; /* mm/s */
static const double gcode_prepare_velocity_Z = 25;  /* mm/s */

static const double EXTERNAL_TRIGGER_DELAY = 0.1;

static ktime_t start_time_s;

enum {
	GCODE_INIT,
	GCODE_WAIT_START,
	GCODE_WAIT_OPT,
	GCODE_START,
	GCODE_READ_FIRST,
	GCODE_PREPARE_Z_OUT,
	GCODE_PREPARE_X,
	GCODE_PREPARE_Y,
	GCODE_PREPARE_Z,
	GCODE_WAIT_BEGIN_MACHINING,
	GCODE_DELAY_SAMPLER_BEFORE_TRIGGER,
	GCODE_DELAY_EXTERNAL_TRIGGER,
	GCODE_RUN,
	GCODE_OUT_Z,
	GCODE_OUT_X,
	GCODE_OUT_Y,
	GCODE_ERROR,
	GCODE__COUNT
};

static int gcode_init(void);
static int gcode_wait_start(void);
static int gcode_wait_opt(void);
static int gcode_start(void);
static int gcode_read_first(void);
static int gcode_prepare_z_out(void);
static int gcode_prepare_x(void);
static int gcode_prepare_y(void);
static int gcode_prepare_z(void);
static int gcode_wait_begin_machining(void);
static int stream_delay_sampler_before_trigger(void);
static int stream_delay_ext_trigger(void);
static int gcode_run(void);
static int gcode_out_x(void);
static int gcode_out_y(void);
static int gcode_out_z(void);
static int gcode_error(void);

DECLARE_FSM(gcode, GCODE__COUNT);
static FSM(
    gcode) fsm_gcode = {/**/
                        .state = GCODE_INIT,
                        .rules = {
                            [GCODE_INIT] = FSM_CB(gcode_init),
                            [GCODE_WAIT_START] = FSM_CB(gcode_wait_start),
                            [GCODE_WAIT_OPT] = FSM_CB(gcode_wait_opt),
                            [GCODE_START] = FSM_CB(gcode_start),
                            [GCODE_READ_FIRST] = FSM_CB(gcode_read_first),
                            [GCODE_PREPARE_Z_OUT] = FSM_CB(gcode_prepare_z_out),
                            [GCODE_PREPARE_X] = FSM_CB(gcode_prepare_x),
                            [GCODE_PREPARE_Y] = FSM_CB(gcode_prepare_y),
                            [GCODE_PREPARE_Z] = FSM_CB(gcode_prepare_z),
							[GCODE_WAIT_BEGIN_MACHINING] = FSM_CB(gcode_wait_begin_machining),
							[GCODE_DELAY_SAMPLER_BEFORE_TRIGGER] = FSM_CB(stream_delay_sampler_before_trigger),
							[GCODE_DELAY_EXTERNAL_TRIGGER] = FSM_CB(stream_delay_ext_trigger),
                            [GCODE_RUN] = FSM_CB(gcode_run),
                            [GCODE_OUT_Z] = FSM_CB(gcode_out_z),
                            [GCODE_OUT_X] = FSM_CB(gcode_out_x),
                            [GCODE_OUT_Y] = FSM_CB(gcode_out_y),
                            [GCODE_ERROR] = FSM_CB(gcode_error),
                        }};

static const pin_def_t pin_def[] = {
    {HAL_BIT, HAL_IN, PIN(start_in), "lcct.gcode.start-in"},
    {HAL_BIT, HAL_IN, PIN(pause_in), "lcct.gcode.pause-in"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_rt_pause_out), "lcct.gcode.feedopt-rt-pause"},
    {HAL_BIT, HAL_IN, PIN(feedopt_ready_in), "lcct.gcode.feedopt-ready"},
    {HAL_BIT, HAL_IN, PIN(feedopt_finished_in), "lcct.gcode.feedopt-rt-finished"},
    {HAL_BIT, HAL_IN, PIN(feedopt_underrun_in), "lcct.gcode.feedopt-rt-underrun"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_single_shot_out), "lcct.gcode.feedopt-single-shot"},
    {HAL_BIT, HAL_IN, PIN(feedopt_us_active_in), "lcct.gcode.feedopt-us-active"},
    {HAL_BIT, HAL_IN, PIN(feedopt_rt_active_in), "lcct.gcode.feedopt-rt-active"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_us_start_out), "lcct.gcode.feedopt-us-start"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_rt_start_out), "lcct.gcode.feedopt-rt-start"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_us_reset_out), "lcct.gcode.feedopt-us-reset"},
    {HAL_BIT, HAL_OUT, PIN(feedopt_rt_reset_out), "lcct.gcode.feedopt-rt-reset"},
    {HAL_BIT, HAL_IN, PIN(feedopt_rt_has_segment), "lcct.gcode.feedopt-rt-has-segment"},
    {HAL_BIT, HAL_IN, PIN(feedopt_reset_in), "lcct.gcode.feedopt-reset"},
    {HAL_FLOAT, HAL_IN, PIN(joint_pos_cmd_in[0]), "lcct.gcode.joint-pos-cmd-0"},
    {HAL_FLOAT, HAL_IN, PIN(joint_pos_cmd_in[1]), "lcct.gcode.joint-pos-cmd-1"},
    {HAL_FLOAT, HAL_IN, PIN(joint_pos_cmd_in[2]), "lcct.gcode.joint-pos-cmd-2"},
	{HAL_S32, HAL_OUT, PIN(lcct_gcode_runtime_s), "lcct.gcode.runtime-s"},
    {HAL_BIT, HAL_OUT, PIN(lcct_gcode_done), "lcct.gcode.done"},
    {HAL_FLOAT, HAL_IN, PIN(feedopt_spindle_speed), "lcct.gcode.feedopt-spindle-speed"},

	{HAL_TYPE_UNSPECIFIED}};

int lcct_gcode_init(int comp_id)
{
	HAL_INIT_PINS(pin_def, comp_id, data);
	add_hal_button(data->start_in);
	add_hal_button(data->pause_in);
	add_hal_button(data->feedopt_reset_in);
	return 0;
}

void send_reset(void) {
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: send_reset\n");
    *data->feedopt_us_reset_out = 1;
    *data->feedopt_rt_reset_out = 1;
    *data->feedopt_rt_start_out = 0;
    *data->feedopt_rt_pause_out = 0;
}

void lcct_gcode_reset(void)
{
    send_reset();
    fsm_gcode.state = GCODE_INIT;
}

static int gcode_init(void)
{
	set_mode_csp(AXIS_X | AXIS_Y | AXIS_Z);
	ext_trigger_enable(0);
    set_spindle_speed(0);

	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: WaitStart\n");
	return GCODE_WAIT_START;
}

static int gcode_wait_start(void)
{
    if (*data->feedopt_reset_in) {
        send_reset();
        return GCODE_INIT;
    }
	if (*data->feedopt_ready_in) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: WaitOpt\n");
		return GCODE_WAIT_OPT;
	}
	return GCODE_WAIT_START;
}

static int gcode_wait_opt(void)
{
    if (*data->feedopt_reset_in) {
        send_reset();
        return GCODE_INIT;
    }

	if (*data->feedopt_ready_in && *data->start_in) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: Start\n");
		offsetX = get_offset(AXIS_X);
		offsetY = get_offset(AXIS_Y);
		offsetZ = get_offset(AXIS_Z);
		offsetThetaZ = get_offset_thetaZ();

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
#warning Disabled code: cos/sin not implemented in ARM targets !
#else
/* gitlab issue [lcct] Compilation failed on ARM */
		offsetCosTheta = cos(offsetThetaZ * M_PI / 180.0);
		offsetSinTheta = sin(offsetThetaZ * M_PI / 180.0);
#endif /* CONFIG_ARM */

		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "GCODE: Using offsets: x = %f, y = %f, z = %f\n", offsetX, offsetY, offsetZ);
		return GCODE_START;
	}
	return GCODE_WAIT_OPT;
}

static int gcode_start(void)
{
	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: ReadFirst\n");
	*data->feedopt_single_shot_out = 1;
	return GCODE_READ_FIRST;
}

static int gcode_read_first(void)
{
	gcode_target_x = *data->joint_pos_cmd_in[AXIS_X_OFFSET] + offsetX;
	gcode_target_y = *data->joint_pos_cmd_in[AXIS_Y_OFFSET] + offsetY;
	gcode_target_z = -*data->joint_pos_cmd_in[AXIS_Z_OFFSET] + offsetZ;

    set_spindle_speed(*data->feedopt_spindle_speed);

	gcode_initial_x = get_position(AXIS_X);
	gcode_initial_y = get_position(AXIS_Y);
	gcode_initial_z = get_position(AXIS_Z);

    gcode_prepare_time_x =
        LCCT_MAX((fabs(gcode_target_x - gcode_initial_x) / gcode_prepare_velocity_XY), 0.1);
    gcode_prepare_time_y =
        LCCT_MAX((fabs(gcode_target_y - gcode_initial_y) / gcode_prepare_velocity_XY), 0.1);
    gcode_prepare_time_z =
        LCCT_MAX((fabs(gcode_target_z - gcode_initial_z) / gcode_prepare_velocity_Z), 0.1);


	RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: PREPARE_Z_OUT\n");

    cmd_move_axis_abs(AXIS_Z, get_home_pos_z(), gcode_prepare_velocity_Z);
    return GCODE_PREPARE_Z_OUT;
}

static int gcode_prepare_z_out(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: PREPARE_X\n");
        cmd_move_axis_abs(AXIS_X, gcode_target_x, gcode_prepare_velocity_XY);
		return GCODE_PREPARE_X;
	}
	return GCODE_PREPARE_Z_OUT;
}

static int gcode_prepare_x(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: PREPARE_Y\n");
		cmd_move_axis_abs(AXIS_Y, gcode_target_y, gcode_prepare_velocity_XY);
		return GCODE_PREPARE_Y;
	}
	return GCODE_PREPARE_X;
}

static int gcode_prepare_y(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: PREPARE_Z\n");
		cmd_move_axis_abs(AXIS_Z, gcode_target_z, gcode_prepare_velocity_Z);
		return GCODE_PREPARE_Z;
	}
	return GCODE_PREPARE_Y;
}

static int gcode_prepare_z(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: WaitBegin\n");
		return GCODE_WAIT_BEGIN_MACHINING;
	}
	return GCODE_PREPARE_Z;
}

static int gcode_wait_begin_machining(void)
{
    if (*data->feedopt_reset_in) {
        send_reset();
        return GCODE_RUN;
    }

	if (*data->start_in) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: Run\n");
		gcode_init_t0 = lcct_time();
		start_time_s = ktime_get();
		sampler_enable(1);
		return GCODE_DELAY_SAMPLER_BEFORE_TRIGGER;
	}
	return GCODE_WAIT_BEGIN_MACHINING;
}

static int stream_delay_sampler_before_trigger(void)
{
	if (lcct_time() - gcode_init_t0 > EXTERNAL_TRIGGER_DELAY) {
		ext_trigger_enable(1);
		gcode_init_t0 = lcct_time();
		return GCODE_DELAY_EXTERNAL_TRIGGER;
	}
	return GCODE_DELAY_SAMPLER_BEFORE_TRIGGER;
}

static int stream_delay_ext_trigger(void)
{
	if (lcct_time() - gcode_init_t0 > EXTERNAL_TRIGGER_DELAY) {
		*data->feedopt_rt_start_out = 1;
		return GCODE_RUN;
	}
	return GCODE_DELAY_EXTERNAL_TRIGGER;
}

static int gcode_run(void)
{
#if !defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
	double x0;
	double y0;
	double z0;
	double x;
	double y;
	double z;
#endif

	*data->lcct_gcode_runtime_s = ktime_get() - start_time_s;

    if (*data->feedopt_finished_in) {
        sampler_enable(0);
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: OutZ\n");
		cmd_move_axis_abs(AXIS_Z, get_home_pos_z(), gcode_prepare_velocity_Z);
		*data->lcct_gcode_done = true;
		return GCODE_OUT_Z;
	}

    else if (*data->feedopt_reset_in) {
        sampler_enable(0);
        RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: OutZ\n");
        cmd_move_axis_abs(AXIS_Z, get_home_pos_z(), gcode_prepare_velocity_Z);
        *data->lcct_gcode_done = true;
        send_reset();
        return GCODE_OUT_Z;
    }

    if (*data->feedopt_underrun_in) {
        RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "LCCT_GCODE: UNDERRUN\n");
        return GCODE_ERROR;
    }

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	set_position(AXIS_X, *data->joint_pos_cmd_in[AXIS_X_OFFSET] + offsetX);
	set_position(AXIS_Y, *data->joint_pos_cmd_in[AXIS_Y_OFFSET] + offsetY);
	set_position(AXIS_Z, -*data->joint_pos_cmd_in[AXIS_Z_OFFSET] + offsetZ);
#else /* defined(CONFIG_ARM) || defined(CONFIG_ARM64) */
	x0 = *data->joint_pos_cmd_in[AXIS_X_OFFSET];
	y0 = *data->joint_pos_cmd_in[AXIS_Y_OFFSET];
	z0 = *data->joint_pos_cmd_in[AXIS_Z_OFFSET];

	/* 2D rotation:
	 * [ cos -sin ] * [ x ]
	 * [ sin  cos ]   [ y ]
	 */
	x = offsetCosTheta * x0 - offsetSinTheta * y0;
	y = offsetSinTheta * x0 + offsetCosTheta * y0;
 	z = z0;

 	set_position(AXIS_X, x + offsetX);
	set_position(AXIS_Y, y + offsetY);
	set_position(AXIS_Z, z + offsetZ);
#endif /* else defined(CONFIG_ARM) || defined(CONFIG_ARM64) */
    set_spindle_speed(*data->feedopt_spindle_speed);

	/* forward pause ... */
	if (*data->pause_in) {
		*data->feedopt_rt_pause_out = 1;
 	}

    /* ... and start to feedopt */
    if (*data->start_in) {
        *data->feedopt_rt_start_out = 1;
	    *data->feedopt_rt_pause_out = 0;
    }

	/* if the spindle slows down too much, stop machining	*/
    if ((fabs(get_target_spindle_speed() - get_spindle_speed()) /
         (get_target_spindle_speed() + 1)) > get_spindle_threshold()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "Spindle speed decreased by more than 30%%, aborting\n");
        return GCODE_ERROR;
	}

	return GCODE_RUN;
}

static int gcode_out_z(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: OutX\n");
		cmd_move_axis_abs(AXIS_X, get_home_pos_x(), gcode_prepare_velocity_XY);
		return GCODE_OUT_X;
	}
	return GCODE_OUT_Z;
}

static int gcode_out_x(void)
{
	if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: OutY\n");
		cmd_move_axis_abs(AXIS_Y, get_home_pos_y(), gcode_prepare_velocity_XY);
		return GCODE_OUT_Y;
	}
	return GCODE_OUT_X;
}

static int gcode_out_y(void)
{
    if (cmd_done()) {
		RTAPI_PRINT_MSG(RTAPI_MSG_DBG, "LCCT_GCODE: Init\n");
		return GCODE_INIT;
	}
	return GCODE_OUT_Y;
}

static int gcode_error(void)
{
    if (*data->feedopt_reset_in) {
        cmd_move_axis_abs(AXIS_Z, get_home_pos_z(), gcode_prepare_velocity_Z);
        *data->lcct_gcode_done = true;
        send_reset();
        return GCODE_OUT_Z;
    }
    return GCODE_ERROR;
}

FSM_STATUS lcct_gcode(void)
{
    FSM_UPDATE(fsm_gcode)

    if (fsm_gcode.state == GCODE_INIT || fsm_gcode.state == GCODE_WAIT_START ||
        fsm_gcode.state == GCODE_WAIT_OPT) {
		return FSM_FINISHED;
	} else {
		return FSM_CONTINUE;
	}
}
