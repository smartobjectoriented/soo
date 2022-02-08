/**
 * @file lcct_internal.h
 * @brief Defines a number of functions used by all LCCT sub-modules
 */

#ifndef LCCT_INTERNAL_H
#define LCCT_INTERNAL_H

#include <linux/types.h>

#ifdef CONFIG_X86
#include <asm/fpu/api.h>
#endif

#include <linux/fs.h>

#include <opencn/rtapi/rtapi.h>		/* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h> /* RTAPI realtime module decls */
#include <opencn/rtapi/rtapi_math.h>

#include <opencn/ctypes/strings.h>

#include <opencn/hal/hal.h> /* HAL public API decls */

#define LCCT_MAX(A, B) ((A) > (B) ? (A) : (B))
#define SIGN(v) (v >= 0 ? 1 : -1)

typedef int (*fsm_callback_t)(void);

/**
 * @def DECLARE_FSM
 * @brief Create a definition for a new state machine with a type name and number of states.
 * @param name: Type name identifier
 * @param nstates: Number of valid states
 */
#define DECLARE_FSM(name, nstates)                                                                 \
    typedef struct fsm {                                                                           \
        fsm_callback_t rules[nstates];                                                             \
        int state;                                                                                 \
	} fsm_t_##name

#define FSM_CB(fnname) ((fsm_callback_t)fnname)

#define FSM(name) fsm_t_##name

#define FSM_SIZE(var) ((int)(sizeof(var.rules) / sizeof(fsm_callback_t)))

#define FSM_STATE_VALID(fsm_var) (fsm_var.state >= 0 && fsm_var.state < FSM_SIZE(fsm_var))

#define FSM_UPDATE(fsm_var)                                                                        \
    if (FSM_STATE_VALID(fsm_var)) {                                                                \
        if (fsm_var.rules[fsm_var.state] != NULL) {                                                \
            fsm_var.state = fsm_var.rules[fsm_var.state]();                                        \
            if (!FSM_STATE_VALID(fsm_var)) {                                                       \
                rtapi_print_msg(RTAPI_MSG_ERR, "FSM: INVALID STATE RETURNED: %d\n",                \
                                fsm_var.state);                                                    \
                fsm_var.state = -1;                                                                \
            }                                                                                      \
        } else {                                                                                   \
            rtapi_print_msg(RTAPI_MSG_ERR, "FSM: state %d has no callback\n", fsm_var.state);      \
            fsm_var.state = -1;                                                                    \
        }                                                                                          \
	}

typedef enum { MAIN_IDLE, MAIN_HOMING, MAIN_STREAM, MAIN_GCODE, MAIN_JOG, MAIN_ERR } MAIN_STATE;

typedef struct {
    /* Joint position input sources */
    hal_float_t *spindle_cmd_in;

    hal_float_t *joint_pos_cur_in[4];
    hal_float_t *spindle_cur_in;

    hal_bit_t *homed_out;

    /* Joint position output */
    hal_float_t *target_position_out[4];
    hal_float_t *spindle_cmd_out;
    hal_float_t *gui_spindle_target_velocity;
    hal_bit_t *spindle_active;

    hal_bit_t *disable_abs_jog;

    /* Axis mode input */
    hal_bit_t *in_mode_csp_in[4];
    hal_bit_t *in_mode_csv_in[4];
    hal_bit_t *in_mode_hm_in[4];
    hal_bit_t *in_mode_inactive_in[4];

    /* Axis mode output */
    hal_bit_t *set_mode_csp_out[4];
    hal_bit_t *set_mode_csv_out[4];
    hal_bit_t *set_mode_hm_out[4];
    hal_bit_t *set_mode_inactive_out[4];

    /* Faults */
    hal_bit_t *in_fault_in[4];

    /* Offset double controls */
    hal_float_t *spinbox_offset_in[3];
    hal_float_t *home_position_in[3];

    hal_float_t *spinbox_offset_thetaZ;

    /* machine mode */
    hal_bit_t *set_machine_mode_gcode_in;
    hal_bit_t *set_machine_mode_stream_in;
    hal_bit_t *set_machine_mode_homing_in;
    hal_bit_t *set_machine_mode_jog_in;
    hal_bit_t *set_machine_mode_inactive_in;

    /* machine mode leds */
    hal_bit_t *in_machine_mode_gcode_out;
    hal_bit_t *in_machine_mode_stream_out;
    hal_bit_t *in_machine_mode_homing_out;
    hal_bit_t *in_machine_mode_jog_out;
    hal_bit_t *in_machine_mode_inactive_out;

    hal_bit_t *homing_finished_out;
    hal_bit_t *gcode_finished_out, *gcode_running_out;
    hal_bit_t *stream_finished_out, *stream_running_out;
    hal_bit_t *jog_finished_out;

    /* sampling control */
    hal_bit_t *sampler_enable_out;

    hal_bit_t *fault_reset_in;
    hal_bit_t *fault_reset_out[4];

    hal_s32_t *const external_trigger_out;
    hal_s32_t *const electrovalve_in;
    hal_s32_t *const electrovalve_out;
    hal_float_t *const spindle_cur_out;
    hal_float_t *const spindle_threshold;
    hal_float_t *const spindle_acceleration;    /* Spindle acceleration in RPM/s */

} lcct_data_t;

typedef struct {
    double x,y,z,rz;
} transform_t;

transform_t unit_transform(void);

/**
 * @enum FSM_STATUS
 * @brief Finite State Machine exit status
 */
typedef enum {
    FSM_CONTINUE, ///< The state machine is not done
    FSM_FINISHED, ///< The state machine is in a state where it can be reset without issues
    FSM_ERROR	 ///< There was an error, only a reset can solve it
} FSM_STATUS;

typedef enum { DEF_TYPE_END, DEF_TYPE_ONE_AXIS, DEF_TYPE_THREE_AXES, DEF_TYPE_ALL_AXES } DEF_TYPE;

/**
 * @struct pin_def_t
 * @brief Used for describing a hal pin to be created
 */
typedef struct {
    hal_type_t pin_type;   ///< Pin value type
    hal_pin_dir_t pin_dir; ///< Input/Output
    int off;			   ///< Offset from the base pointer
    const char *name;	  ///< Name of the pin
} pin_def_t;

#define HAL_PINDEF_END {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, 0, ""}

/** @def HAL_INIT_PINS
 * @param defs: @ref pin_def_t array
 * @param comp_id: component id
 * @param base_ptr: Pointer to struct containing the pins
 * @return Returns non-zero if an error occured
 * @brief Wrapper arround @ref init_pins_from_def, allocating the base struct and performing error
 * checking
 */
#define HAL_INIT_PINS(defs, comp_id, base_ptr)                                                     \
    {                                                                                              \
        int _ret;                                                                                  \
        if ((base_ptr = hal_malloc(__core_hal_user, sizeof(*base_ptr))) == NULL) {                 \
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL: hal_malloc for base of size %lu failed\n",        \
                            (long unsigned int)sizeof(*base_ptr));                                 \
            return -EINVAL;                                                                        \
        }                                                                                          \
        _ret = init_pins_from_def(defs, comp_id, base_ptr);                                        \
        if (_ret != 0)                                                                             \
            return _ret;                                                                           \
    }

/**
 * @brief Uses a 'pin_type == HAL_TYPE_UNSPECIFIED'-terminated array to initialize hal pins
 * @param defs: @ref pin_def_t array
 * @param comp_id: component id
 * @param base_ptr: Pointer to struct containing the pins
 * @return Returns non-zero if an error occured
 */
int init_pins_from_def(const pin_def_t *defs, int comp_id, void *base_ptr);

/**
 * @enum AXES_ENUM
 * @brief Defines bit flags for all axes in the system
 */
typedef enum {
	AXIS_NONE = 0,
	AXIS_X = 1 << 0,
	AXIS_Y = 1 << 1,
	AXIS_Z = 1 << 2,
	AXIS_W = 1 << 3,
	AXIS_ALL = AXIS_X | AXIS_Y | AXIS_Z | AXIS_W
} AXES_ENUM;

enum { AXIS_X_OFFSET = 0, AXIS_Y_OFFSET = 1, AXIS_Z_OFFSET = 2, AXIS_W_OFFSET = 3, AXIS__COUNT };

/**
 * @brief Adds a HAL_BIT pin to a list of pins that are reset at the end of each update cycle.
 * @details This allows an easy interface for using "buttons". For example a GUI can set a pin to 1,
 *  and then LCCT sets it back to 0 to avoid duplicating events.
 * @param pin: Pin pointer
 */
void add_hal_button(hal_bit_t *pin);

int axis_offset(AXES_ENUM axis);

/**
 * @brief Returns the current position of the axis retrieved from LCEC
 * @param axis
 * @return Position in mm
 * @attention Only valid when homed
 */
double get_position(AXES_ENUM axis);

/**
 * @brief Returns the offset used when following the position from stream from the streamer file or
 * gcode source
 * @param axis: Target axis
 * @return Offset in mm
 */
double get_offset(AXES_ENUM axis);

double get_offset_thetaZ(void);

/**
 * @brief Sets the target position of the specified axis
 * @param axis: Target axis
 * @param value: Position in mm
 * @note This value is checked for big jumps and workspace limits
 */
void set_position(AXES_ENUM axis, double value);



void set_transform(transform_t tform);


/**
 * @brief Returns the target spindle angular velocity
 * @return Angular velocity in rpm
 */
double get_target_spindle_speed(void);

/**
 * @brief Returns the current angular velocity
 * @return Angular velocity in rpm
 */
double get_spindle_speed(void);

/**
 * @brief Switch axes into the position-following mode
 * @param flags: Axes selector (use @ref AXES_ENUM)
 */
void set_mode_csp(int flags);

/**
 * @brief Switch axes into the velocity-following mode
 * @param flags: Axes selector (use @ref AXES_ENUM)
 */
void set_mode_csv(int flags);

/**
 * @brief Turn off the axes
 * @param flags: Axes selector (use @ref AXES_ENUM)
 */
void set_mode_inactive(int flags);

/**
 * @brief Switch axes into the homing mode
 * @param flags: Axes selector (use @ref AXES_ENUM)
 */
void set_mode_hm(int flags);

/**
 * @brief Returns the threshold for detecting spindle slowdown
 * @return
 */
double get_spindle_threshold(void);


/**
 * @brief Returns the maximal spindle acceleration for ramping up
 * @return Acceleration in turns/s/s
 */
double get_spindle_acceleration(void);

/**
 * @brief Issue a relative movement command to an axis (cos profile)
 * @param axis: Target axis
 * @param offset: Offset from current axis position in mm
 * @param speed: Equivalent speed as if the profile was linear in mm/s
 * @note This command will be ignored if the previous one was not completed, check with @ref
 * cmd_done
 */
void cmd_move_axis_rel(AXES_ENUM axis, double offset, double speed);

/**
 * @brief Issue an absolute movement command to an axis (cos profile)
 * @param axis: Target axis
 * @param target_position: Target axis position in mm
 * @param speed: Equivalent as if the profile was linear in mm/s
 * @note This command will be ignored if the previous one was not completed, check with @ref
 * cmd_done
 */
void cmd_move_axis_abs(AXES_ENUM axis, double target_position, double speed);

/**
 * @brief Set the target speed of the spindle
 * @param speed: Spindle speed in RPM
 * @note This command will be ignored if speed is overriden by the gui
 */
void set_spindle_speed(double speed);

/**
 * @brief Returns the spindle speed requested on lcct.gui.spindle-target-velocity
 * @return Spindle speed in RPM
 */
double get_external_spindle_target_speed(void);

/**
 * @brief Has the last command completed?
 * @return status
 */
int cmd_done(void);

/**
 * @brief Set current command to finished
 */
void set_command_finished(void);

/**
 * @brief Stop any ongoing command
 */
void cmd_stop(void);

/**
 * @brief Get current time as measured by LCCT
 * @return Time in seconds
 */
double lcct_time(void);

/**
 * @brief Enable the sampler
 * @param b: State
 */
void sampler_enable(int b);


/**
 * @brief Enable the external trigger
 * @param b: State
 */
void ext_trigger_enable(int b);

/**
 * @brief Get the home position of axis X
 * @return Position in mm
 */
double get_home_pos_x(void);

/**
 * @brief Get the home position of axis Y
 * @return Position in mm
 */
double get_home_pos_y(void);

/**
 * @brief Get the home position of axis Z
 * @return Position in mm
 */
double get_home_pos_z(void);

/**
 * @brief main station initialization
 */
void main_state_init(void);

#endif
