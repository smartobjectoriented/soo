/********************************************************************
 *  Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include <opencn/rtapi/rtapi_math.h>

#include <opencn/uapi/lcec.h>

#include "lcec_priv.h"
#include "lcec_tsd80e.h"

#define TSD_MSG_PFX "TSD: "

#define DECIM_CALL(N, fn)               \
	{                                   \
		static int count = 0;           \
		if (count++ % N == 0) fn;       \
	}

#define DBG(...)
// #define DBG(...) DECIM_CALL(10000, rtapi_print_msg(RTAPI_MSG_DBG, __VA_ARGS__))
// typedef int bool;

/*
 * Vendor ID:       0x0000abba
 * Product code:    0x00000171
 * Revision number: 0x00000001
 */

/* max TargetVelocity: ceil(1300*2.0*3.0*60.0/2.0/pi)=74485
 * ceil(log(74485)/log(2))=17 : need 17 + 1(sign) bits for the velocity command,
 * this leaves 32-18=14 bits for the decimal part
 */
#define VELOCITY_SCALE ((double)(1 << 14)*2.0*3.0*10.0*6.0/2.0/M_PI)

/* max position: +- 64 mm -> 6+1 bits for position command
 * this leaves 32-7=25 bits for the decimal part
 */
#define POSITION_SCALE ((double)(1 << 25))

/* the values 1 << 14 and 1 << 25 MUST match in tsd80e_initcmds.xml
 * !!! ENDIANNESS IS REVERSED IN THAT FILE !!!
*/

typedef void (*hal_func)(void *, long);

/* CiA402 drive profile control word bits */
enum {
	CIA402_CW_SWITCH_ON = 1 << 0,      /* M */
	CIA402_CW_ENABLE_VOLTAGE = 1 << 1, /* M */
	CIA402_CW_QUICK_STOP = 1 << 2,     /* O */
	CIA402_CW_ENABLE_OP = 1 << 3,      /* M */
	CIA402_CW_OP_MODE_BIT_4 = 1 << 4,  /* O */
	CIA402_CW_OP_MODE_BIT_5 = 1 << 5,  /* M */
	CIA402_CW_OP_MODE_BIT_6 = 1 << 6,  /* O */
	CIA402_CW_FAULT_RESET = 1 << 7,    /* O */
	CIA402_CW_HALT = 1 << 8,           /* O */
	CIA402_CW_OP_MODE_BIT_9 = 1 << 9   /* O */
};

/* TSD80e specific control word bits */
enum {
	CW_SWITCH_ON = 1 << 0, //
	CW_ENABLE_VOLTAGE = 1 << 1,
	CW_QUICK_STOP = 1 << 2,
	CW_ENABLE_OP = 1 << 3,

	CW_HM_START_HOMING = 1 << 4,
	CW_FAULT_RESET = 1 << 7,
	CW_HM_HALT = 1 << 8,

	CW_CSP_OUTPUT_CYCLE_COUNTER_0 = 1 << 5,
	CW_CSP_OUTPUT_CYCLE_COUNTER_1 = 1 << 6,
};

/* CiA402 drive profile status word bits */
enum {
	CIA402_SW_READY_TO_SWITCH_ON = 1 << 0,     /* M */
	CIA402_SW_SWITCH_ON = 1 << 1,              /* M */
	CIA402_SW_OP_ENABLED = 1 << 2,             /* M */
	CIA402_SW_FAULT = 1 << 3,                  /* M */
	CIA402_SW_VOLTAGE_ENABLED = 1 << 4,        /* O */
	CIA402_SW_QUICK_STOP = 1 << 5,             /* O */
	CIA402_SW_SWITCH_ON_DISABLED = 1 << 6,     /* M */
	CIA402_SW_WARNING = 1 << 7,                /* O */
	CIA402_SW_MAN_SPEC_8 = 1 << 8,             /* O */
	CIA402_SW_REMOTE = 1 << 9,                 /* O */
	CIA402_SW_OP_MODE_SPEC_10 = 1 << 10,       /* O */
	CIA402_SW_INTERNAL_LIMIT_ACTIVE = 1 << 11, /* O */
	CIA402_SW_OP_MODE_SPEC_12 = 1 << 12,       /* C, M for csp or csv or cst */
	CIA402_SW_OP_MODE_SPEC_13 = 1 << 13,       /* O */
	CIA402_SW_MAN_SPEC_14 = 1 << 14,           /* O */
	CIA402_SW_MAN_SPEC_15 = 1 << 15            /* O */
};

/* TSD80e specific status word bits (some from the standard are not used) */
enum {
	/* common flags */
	SW_READY_TO_SWITCH_ON = 1 << 0, //
	SW_SWITCHED_ON = 1 << 1,
	SW_OP_ENABLED = 1 << 2,
	SW_FAULT = 1 << 3,
	SW_QUICK_STOP = 1 << 5,
	SW_SWITCH_ON_DISABLED = 1 << 6,
	SW_WARNING = 1 << 7,
	SW_FOLLOW_ME = 1 << 8,
	SW_INTERNAL_LIMIT_ACTIVE = 1 << 11,
	SW_REFERENCE_DONE = 1 << 15,

	/* csp mode flags */
	SW_CSP_TOGGLE_STATUS = 1 << 10,
	SW_CSP_DRIVE_FOLLOWS_CMD = 1 << 12,
	SW_CSP_EXTENDED_TOGGLE = 1 << 13,

	/* hm mode flags */
	SW_HM_TARGET_REACHED = 1 << 10,
	SW_HM_HOMING_DONE = 1 << 12,
	SW_HM_HOMING_ERROR = 1 << 13
};

/* CiA402 drive profile modes of operation */
typedef enum {
	CIA402_OP_MODE_NONE = 0,  /* Custom mode for disabling the drive, not in the standard */
	CIA402_OP_MODE_PP = 1,    /* O: Profile position mode */
	CIA402_OP_MODE_VL = 2,    /* O: Velocity mode */
	CIA402_OP_MODE_PV = 3,    /* O: Profile velocity mode */
	CIA402_OP_MODE_TQ = 4,    /* O: Torque profile mode */
	CIA402_OP_MODE_HM = 6,    /* O: Homing mode */
	CIA402_OP_MODE_IP = 7,    /* O: Interpolated position mode */
	CIA402_OP_MODE_CSP = 8,   /* C: Cyclic synchronous position mode  (CSP or CSV */
							  /* or CST is mandatory) */
	CIA402_OP_MODE_CSV = 9,   /* C: Cyclic synchronous velocity mode */
	CIA402_OP_MODE_CST = 10,  /* C: Cyclic synchronous torque mode */
	CIA402_OP_MODE_CSTCA = 11 /* O: Cyclic synchronous torque mode with commutation angle */
} cia402_op_mode_t;

/* TSD80e supported drive modes (0x6502:00 SDO): 0xA1 -> pp,hm,csp
 * status word when turned on  : 0x00e0
 * status word when in csp mode: 0x1027
 * status word when in hm  mode: 0x0527  bit10, op mode specific is active
 * status word when in pp  mode: 0x0523
 * status word when overwrite control : 0x0121
 * status word when enabled in overwrite: 0x0127
 */

ec_pdo_entry_info_t slave_tsd80e_rxpdo_axis_0[] = {
	{0x6040, 0x00, 16}, /* Control Word */
	{0x6060, 0x00, 8},  /* Modes of Operation */
	{0x0000, 0x00, 8},  /* Gap */
	{0x607a, 0x00, 32}, /* Target Position */
	{0x60ff, 0x00, 32}, /* Target Velocity */
};

ec_pdo_entry_info_t slave_tsd80e_rxpdo_axis_1[] = {
	{0x6840, 0x00, 16}, /* Control Word */
	{0x6860, 0x00, 8},  /* Modes of Operation */
	{0x0800, 0x00, 8},  /* Gap */
	{0x687a, 0x00, 32}, /* Target Position */
	{0x68ff, 0x00, 32}, /* Target Velocity */
};

ec_pdo_entry_info_t slave_tsd80e_txpdo_axis_0[] = {
	{0x6041, 0x00, 16}, /* Status Word */
	{0x6061, 0x00, 8},  /* Modes of Operation Display */
	{0x0000, 0x00, 8},  /* Gap */
	{0x6064, 0x00, 32}, /* Position Actual Value */
	{0x60f4, 0x00, 32}, /* Position Error */
	{0x606c, 0x00, 32}, /* Velocity Actual Value */
	{0x6077, 0x00, 16}, /* Torque Actual Value */
	{0x603f, 0x00, 8},  /* Error Code */
	{0x0000, 0x00, 8},  /* Gap */
};

/* Some usefull values that we could be interested in
ec_pdo_entry_info_t slave_tsd80e_txpdo_extra_backup[] = {
	// AXIS 0
	{0x2301, 0x00, 32}, // ActualCurrentQ
	{0x2304, 0x00, 32}, // DesiredVoltageQ
	{0x2309, 0x00, 32}, // ActualCurrent Phase U
	{0x22B5, 0x00, 32}, // PathInterpolator position
	{0x22B6, 0x00, 32}, // PathInterpolator velocity
	{0x22B7, 0x00, 32}, // PathInterpolator acceleration

	// AXIS 1
	{0x2B01, 0x00, 32}, // ActualCurrentQ
	{0x2B04, 0x00, 32}, // DesiredVoltageQ
	{0x2B09, 0x00, 32}, // ActualCurrent Phase U
	{0x2AB5, 0x00, 32}, // PathInterpolator position
	{0x2AB6, 0x00, 32}, // PathInterpolator velocity
	{0x2AB7, 0x00, 32}, // PathInterpolator acceleration
};*/


/* add the same register to both axes */
#define DUAL_ENTRY(index, subindex, bits)                                                                              \
	{index, subindex, bits}, { index + 0x800, subindex, bits }

/* even entries: axis 0, odd entries: axis 1 */
static ec_pdo_entry_info_t slave_tsd80e_txpdo_extra[8] = {
	DUAL_ENTRY(0x2301, 0x00, 32), /* ActualCurrentQ */
	DUAL_ENTRY(0x2304, 0x00, 32), /* DesiredVoltageQ */
	DUAL_ENTRY(0x22B5, 0x00, 32), /* PathInterpolator position */
	DUAL_ENTRY(0x22B7, 0x00, 32), /* PathInterpolator acceleration */
};


static ec_pdo_entry_info_t slave_tsd80e_rxpdo_extra[4] = {
	DUAL_ENTRY(0x2274, 0x00, 32), /* DigitalOutput0 */
	DUAL_ENTRY(0x2275, 0x00, 32), /* DigitalOutput1 */
};

static ec_pdo_entry_info_t slave_tsd80e_txpdo_axis_1[] = {
	{0x6841, 0x00, 16}, /* Status Word */
	{0x6861, 0x00, 8},  /* Modes of Operation Display */
	{0x0800, 0x00, 8},  /* Gap */
	{0x6864, 0x00, 32}, /* Position Actual Value */
	{0x68f4, 0x00, 32}, /* Position Error */
	{0x686c, 0x00, 32}, /* Velocity Actual Value */
	{0x6877, 0x00, 16}, /* Torque Actual Value */
	{0x683f, 0x00, 8},  /* Error Code */
	{0x0800, 0x00, 8},  /* Gap */
};

static ec_pdo_info_t slave_tsd80e_pdos[] = {
	{0x1604, 5, slave_tsd80e_rxpdo_axis_0}, /* csp/pp RxPDO - axis 0 */
	{0x1614, 5, slave_tsd80e_rxpdo_axis_1}, /* csp/pp RxPDO - axis 1 */
	/* @Hack */
	/* DO NOT USE DIGITAL OUT 0 BECAUSE THE EMBEDDED PROGRAMS IN THE
	 * DRIVES USE THEM FOR ERROR COMMUNICATION */
#if 0
	{0x1623, 1, slave_tsd80e_rxpdo_extra + 0}, {0x1633, 1, slave_tsd80e_rxpdo_extra + 1},
	{0x1643, 1, slave_tsd80e_rxpdo_extra + 2}, {0x1653, 1, slave_tsd80e_rxpdo_extra + 3},
#else
	/* @Hack */
	/* Usable PDOs: 1623, 1633, 1643, 1653 */
	/* Make sure not to leave any holes in PDO usage, or it won't work */
	/* for example: using 1643 and 1653 here is incorrect, because 1623 */
	/* and 1633 are not used*/
	{0x1623, 1, slave_tsd80e_rxpdo_extra + 2}, {0x1633, 1, slave_tsd80e_rxpdo_extra + 3},
#endif

	{0x1a01, 9, slave_tsd80e_txpdo_axis_0}, /* csp/pp TxPDO - axis 0 */
	{0x1a11, 9, slave_tsd80e_txpdo_axis_1}, /* csp/pp TxPDO - axis 1 */

	{0x1a23, 2, slave_tsd80e_txpdo_extra + 0}, {0x1a33, 2, slave_tsd80e_txpdo_extra + 2},
	{0x1a43, 2, slave_tsd80e_txpdo_extra + 4}, {0x1a53, 2, slave_tsd80e_txpdo_extra + 6},
	/*{0x1a22, 1, slave_tsd80e_txpdo_extra + 8}, // useless, we can only use 6
	PDOs {0x1a32, 1, slave_tsd80e_txpdo_extra + 9}, {0x1a42, 1,
	slave_tsd80e_txpdo_extra + 10}, {0x1a52, 1, slave_tsd80e_txpdo_extra +
	11},*/
};

static ec_sync_info_t lcec_tsd80e_syncs[] = {
	/* sync manager channel, dir, number of pdos, list pointer, watchdog mode */
	{0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
	{1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 4, slave_tsd80e_pdos + 0, EC_WD_ENABLE}, /* uses 0x1c12 */
	{3, EC_DIR_INPUT, 6, slave_tsd80e_pdos + 4, EC_WD_DISABLE}, /* the maximum is 6, 0x1c13 has only 6 sub indices */
	{0xff}
};

typedef struct {
	 hal_bit_t *test_pin;
} lcec_tsd80e_general_t;

typedef struct lcec_tsd80e_axis lcec_tsd80e_axis_t;

typedef void(lcec_tsd80e_state_t)(lcec_tsd80e_axis_t *axis, uint8_t *pd /*process data*/,
								  bool is_read /*are we in the read part?*/);

struct lcec_tsd80e_axis {
	hal_float_t *target_position_pin;
	hal_float_t *target_velocity_pin;
	hal_float_t *target_acceleration_pin;

	hal_float_t *cur_position_pin;
	hal_float_t *position_err_pin;
	hal_s32_t *cur_torque_pin;
	hal_float_t *cur_velocity_pin;

	hal_bit_t *in_fault_pin, *in_mode_hm_pin, *in_mode_csp_pin, *in_mode_csv_pin, *in_mode_inactive_pin;
	hal_bit_t *set_mode_pp_pin, *set_mode_hm_pin, *set_mode_csp_pin, *set_mode_csv_pin, *set_mode_inactive_pin;
	hal_bit_t *is_homed_pin;
	hal_bit_t *sw_bit_0, *sw_bit_1, *sw_bit_2, *sw_bit_3, *sw_bit_4, *sw_bit_5, *sw_bit_6, *sw_bit_7, *sw_bit_8,
		*sw_bit_9, *sw_bit_10, *sw_bit_11, *sw_bit_12, *sw_bit_13, *sw_bit_14, *sw_bit_15;

	hal_float_t *extra_tx_vars_pins[4];
	hal_s32_t *extra_rx_vars_pins[2];

	hal_bit_t *fault_reset_pin;

	hal_bit_t *do_homing_pin, *do_stop_homing_pin;

	hal_bit_t *sw_bits[16]; /* status word bits */
	hal_bit_t *cw_bits[16]; /* control word bits */

	lcec_tsd80e_state_t *state;
	int32_t current_position;
	int32_t target_pos;
	int32_t target_vel;
	int32_t position_err;
	int32_t cur_torque;
	int32_t cur_velocity;
	uint16_t sw;
	uint16_t cw;
	cia402_op_mode_t target_mode;

	uint32_t ctrl_word_offs;
	uint32_t op_mode_offs;
	uint32_t pos_target_offs;
	uint32_t vel_target_offs;
	uint32_t status_word_offs;
	uint32_t op_mode_display_offs;
	uint32_t cur_position_offs;
	uint32_t position_err_offs;
	uint32_t cur_velocity_offs;
	uint32_t cur_torque_offs;
	uint32_t err_code_offs;
	uint32_t act_current_offs;
	uint32_t digital_output_0_offs;
	uint32_t extra_tx_vars_offs[4];
	uint32_t extra_rx_vars_offs[2];

	uint32_t ctrl_word_bitp;
	uint32_t op_mode_bitp;
	uint32_t pos_target_bitp;
	uint32_t vel_target_bitp;
	uint32_t status_word_bitp;
	uint32_t op_mode_display_bitp;
	uint32_t cur_position_bitp;
	uint32_t position_err_bitp;
	uint32_t cur_velocity_bitp;
	uint32_t cur_torque_bitp;
	uint32_t err_code_bitp;
	uint32_t act_current_bitp;

	uint32_t digital_output_0_bitp;
	uint32_t extra_tx_vars_bitp[4];
	uint32_t extra_rx_vars_bitp[2];
};

static int debug = 0; // Is the master running in debug mode?

static lcec_tsd80e_state_t sm_init, sm_mode_csp, sm_mode_csv, sm_mode_hm, sm_fault;

static const char *state_to_str(lcec_tsd80e_state_t state)
{
	if (state == sm_init) return "SM_INIT";
	if (state == sm_mode_csp) return "SM_MODE_CSP";
	if (state == sm_mode_csv) return "SM_MODE_CSV";
	if (state == sm_mode_hm) return "SM_MODE_HM";
	if (state == sm_fault) return "SM_FAULT";

	return "UNKNOWN STATE";
}

static void axis_set_state(lcec_tsd80e_axis_t *axis, lcec_tsd80e_state_t state)
{
	if (axis->state != state) {
		axis->state = state;
		rtapi_print_msg(RTAPI_MSG_DBG, "axis->state = %s\n", state_to_str(state));
	}
}

static bool axis_has_status(lcec_tsd80e_axis_t *axis, uint16_t status_word_mask)
{
	 return (axis->sw & status_word_mask) == status_word_mask;
}

static void axis_write_cw(lcec_tsd80e_axis_t *axis, uint16_t cw, uint8_t *pd)
{
	if(!debug) EC_WRITE_U16(pd + axis->ctrl_word_offs, cw);
	axis->cw = cw;
}

static void axis_write_mode(lcec_tsd80e_axis_t *axis, cia402_op_mode_t mode, uint8_t *pd)
{
	if (mode == CIA402_OP_MODE_CSP || mode == CIA402_OP_MODE_HM || mode == CIA402_OP_MODE_PP ||
			mode == CIA402_OP_MODE_CSV) {
		if(!debug) EC_WRITE_U8(pd + axis->op_mode_offs, mode);
	} else {
		DBG("MODE %d not supported by the TSD80e!!\n", mode);
	}
}

/* initial state when powering up the drive */
static void sm_init(lcec_tsd80e_axis_t *axis, uint8_t *pd, bool is_read)
{
	if (is_read) {
	*axis->in_mode_hm_pin = 0;
		*axis->in_mode_csp_pin = 0;
		*axis->in_fault_pin = 0;
		*axis->in_mode_csv_pin = 0;
		*axis->in_mode_inactive_pin = 1;
		/* we wait until the drive steps through the states "not ready to switch
		 * on" and "switch on disabled", and ends on the master controlled state
		 * "ready to switch on" as per the implementation directives
		 * (this drive does the steps)
		 */
		if(debug) axis->sw |= SW_READY_TO_SWITCH_ON;
		if (axis_has_status(axis, SW_READY_TO_SWITCH_ON)) {
			if (axis->target_mode == CIA402_OP_MODE_CSP) {
				axis_set_state(axis, sm_mode_csp);
			} else if (axis->target_mode == CIA402_OP_MODE_HM) {
				axis_set_state(axis, sm_mode_hm);
			} else if (axis->target_mode == CIA402_OP_MODE_CSV) {
				axis_set_state(axis, sm_mode_csv);
			}
		}
	} else {
		// use an empty control word for now
		axis_write_cw(axis, 0, pd);
	}
}

/* cycle synchronous position controlled mode/state */
static void sm_mode_csp(lcec_tsd80e_axis_t *axis, uint8_t *pd, bool is_read)
{
	*axis->in_mode_hm_pin = 0;
	if(!debug) *axis->in_mode_csp_pin = axis_has_status(axis, SW_CSP_DRIVE_FOLLOWS_CMD);
	else *axis->in_mode_csp_pin = 1;
	*axis->in_fault_pin = 0;
	*axis->in_mode_csv_pin = 0;
	*axis->in_mode_inactive_pin = 0;
	if (is_read) {

	} else {
		axis_write_mode(axis, CIA402_OP_MODE_CSP, pd);
		axis_write_cw(axis, CW_ENABLE_VOLTAGE | CW_SWITCH_ON | CW_ENABLE_OP, pd);

        axis->target_pos = (int32_t)(*(axis->target_position_pin) * POSITION_SCALE);
		if(!debug) EC_WRITE_S32(&pd[axis->pos_target_offs], axis->target_pos);
	}
}

/* cycle synchronous velocity controller mode/state */
static void sm_mode_csv(lcec_tsd80e_axis_t *axis, uint8_t *pd, bool is_read)
{
	*axis->in_mode_hm_pin = 0;
	*axis->in_mode_csp_pin = 0;
	*axis->in_fault_pin = 0;
	*axis->in_mode_csv_pin = 1;
	*axis->in_mode_inactive_pin = 0;
	if (is_read) {
	} else {

		DBG("vel target offset = %d\n", axis->vel_target_offs);
		axis_write_mode(axis, CIA402_OP_MODE_CSV, pd);
		axis_write_cw(axis, CW_ENABLE_VOLTAGE | CW_SWITCH_ON | CW_ENABLE_OP, pd);
		axis->target_vel = (int32_t)(*axis->target_velocity_pin * VELOCITY_SCALE);
		/*axis->target_vel = (int32_t)(*(axis->target_velocity_pin) * VELOCITY_SCALE); */
		if(!debug) EC_WRITE_S32(&pd[axis->vel_target_offs], axis->target_vel);
	}
}

// homing mode/state
static void sm_mode_hm(lcec_tsd80e_axis_t *axis, uint8_t *pd, bool is_read)
{
	*axis->in_mode_hm_pin = 1;
	*axis->in_mode_csp_pin = 0;
	*axis->in_fault_pin = 0;
	*axis->in_mode_csv_pin = 0;
	*axis->in_mode_inactive_pin = 0;

	if (is_read) {

	} else {
		axis_write_mode(axis, CIA402_OP_MODE_HM, pd);
		axis_write_cw(axis, CW_ENABLE_VOLTAGE | CW_SWITCH_ON | CW_ENABLE_OP, pd);

		if (*axis->do_stop_homing_pin) {
			*axis->do_stop_homing_pin = 0;
			axis_write_cw(axis, CW_ENABLE_VOLTAGE | CW_SWITCH_ON | CW_ENABLE_OP | CW_HM_HALT, pd);
			rtapi_print_msg(RTAPI_MSG_DBG, "STOP HOMING\n");
		}

		else if (*axis->do_homing_pin) {
			*axis->do_homing_pin = 0;
			axis_write_cw(axis, CW_ENABLE_VOLTAGE | CW_SWITCH_ON | CW_ENABLE_OP | CW_HM_START_HOMING, pd);
			rtapi_print_msg(RTAPI_MSG_DBG, "START HOMING\n");
		}
	}
}

/* fault state, the drive stays in this state until the user clears it using the
 * fault-reset-n hal variable, n = axis number
 */
static void sm_fault(lcec_tsd80e_axis_t *axis, uint8_t *pd, bool is_read)
{
	*axis->in_mode_hm_pin = 0;
	*axis->in_mode_csp_pin = 0;
	*axis->in_fault_pin = 1;
	*axis->in_mode_csv_pin = 0;

	if (is_read) {
		DBG("sm_fault: read\n");
		if (!axis_has_status(axis, SW_FAULT)) {
			/* when the fault indicator goes away, reset the drive */
			axis->target_mode = CIA402_OP_MODE_NONE;
			axis_set_state(axis, sm_init);
		}
	} else {
		DBG("sm_fault: write\n");
		if (*axis->fault_reset_pin) {
			axis_write_cw(axis, CW_FAULT_RESET, pd);
			*axis->fault_reset_pin = 0;
		}
	}
}

/** \brief complete data structure for TSD80e */
typedef struct {
	lcec_tsd80e_axis_t axis[LCEC_TSD80E_AXIS];
	hal_u32_t *period_pin;
} lcec_tsd80e_data_t;

static const lcec_pindesc_t slave_pins[] = {
	{HAL_FLOAT, HAL_IN, offsetof(lcec_tsd80e_axis_t, target_position_pin), "%s.%s.%s.target-position-%d"},
	{HAL_FLOAT, HAL_IN, offsetof(lcec_tsd80e_axis_t, target_velocity_pin), "%s.%s.%s.target-velocity-%d"},
#if 0
	//{ HAL_FLOAT, HAL_IN,  offsetof(lcec_tsd80e_axis_t, target_velocity_pin),
	//"%s.%s.%s.target-velocity-%d" },  // not implemented { HAL_FLOAT, HAL_IN,
	// offsetof(lcec_tsd80e_axis_t, target_acceleration_pin),
	//"%s.%s.%s.target-acceleration-%d" }, // the tsd80e does not have
	// acceleration control
#endif
	{HAL_FLOAT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, cur_position_pin), "%s.%s.%s.current-position-%d"},
	{HAL_FLOAT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, cur_velocity_pin), "%s.%s.%s.current-velocity-%d"},
	{HAL_FLOAT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, position_err_pin), "%s.%s.%s.position-error-%d"},
	{HAL_S32, HAL_OUT, offsetof(lcec_tsd80e_axis_t, cur_torque_pin), "%s.%s.%s.current-torque-%d"},

	/* drive mode indicators */
	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, in_mode_hm_pin), "%s.%s.%s.in-mode-hm-%d"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, in_mode_csp_pin), "%s.%s.%s.in-mode-csp-%d"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, in_mode_csv_pin), "%s.%s.%s.in-mode-csv-%d"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, in_mode_inactive_pin), "%s.%s.%s.in-mode-inactive-%d"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, in_fault_pin), "%s.%s.%s.in-fault-%d"},

	/* drive mode setters (the drive resets these to 0 after accepting the command */
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, set_mode_hm_pin), "%s.%s.%s.set-mode-hm-%d"},
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, set_mode_csp_pin), "%s.%s.%s.set-mode-csp-%d"},
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, set_mode_csv_pin), "%s.%s.%s.set-mode-csv-%d"},
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, set_mode_inactive_pin), "%s.%s.%s.set-mode-inactive-%d"},

	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, do_homing_pin), "%s.%s.%s.do-homing-%d"},
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, do_stop_homing_pin), "%s.%s.%s.do-stop-homing-%d"},
	{HAL_BIT, HAL_IN, offsetof(lcec_tsd80e_axis_t, fault_reset_pin), "%s.%s.%s.fault-reset-%d"},

	{HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_axis_t, is_homed_pin), "%s.%s.%s.homed-%d"},
#if 0
	//    { HAL_BIT, HAL_OUT, offsetof(lcec_tsd80e_general_t, test_pin),
	//    "%s.%s.%s.test-pin" },
#endif
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};

void lcec_tsd80e_write(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data; /* process data */
	lcec_tsd80e_data_t *hal_data = (lcec_tsd80e_data_t *)slave->hal_data;
	int extra_var_i;
	int axis_i;

//	DBG("lcec_tsd80e_write\n");

	for (axis_i = 0; axis_i < LCEC_TSD80E_AXIS; axis_i++) {
		lcec_tsd80e_axis_t *axis = &hal_data->axis[axis_i];
		if (*axis->set_mode_hm_pin) {
			axis->target_mode = CIA402_OP_MODE_HM;
			*axis->set_mode_hm_pin = 0;
			axis_set_state(axis, sm_init);
		} else if (*axis->set_mode_csp_pin) {
			axis->target_mode = CIA402_OP_MODE_CSP;
			*axis->set_mode_csp_pin = 0;
			axis_set_state(axis, sm_init);
		} else if (*axis->set_mode_csv_pin) {
			axis->target_mode = CIA402_OP_MODE_CSV;
			*axis->set_mode_csv_pin = 0;
			axis_set_state(axis, sm_init);
		}  else if (*axis->set_mode_inactive_pin) {
			axis->target_mode = CIA402_OP_MODE_NONE;
			*axis->set_mode_inactive_pin = 0;
			axis_set_state(axis, sm_init);
		}
		axis->state(axis, pd, false);



		/* @Hack Don't use digout0s*/
		for (extra_var_i = 1; extra_var_i < 2; ++extra_var_i) {
			if(!debug) *(int *)&pd[axis->extra_rx_vars_offs[extra_var_i]] = *axis->extra_rx_vars_pins[extra_var_i];
		}
	}
}

void lcec_tsd80e_read(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_tsd80e_data_t *hal_data = (lcec_tsd80e_data_t *)slave->hal_data;
	int i;
	int extra_var_i;

//	DBG("lcec_tsd80e_read\n");

	*hal_data->period_pin = period;

	for (i = 0; i < LCEC_TSD80E_AXIS; i++) {
		lcec_tsd80e_axis_t *axis = &hal_data->axis[i];

		/* Read Drive values */
		if(!debug) {
			axis->current_position = EC_READ_S32(&pd[axis->cur_position_offs]);
			axis->sw = EC_READ_U16(&pd[axis->status_word_offs]);
			axis->cur_torque = EC_READ_U32(&pd[axis->cur_torque_offs]);
			axis->position_err = EC_READ_S32(&pd[axis->position_err_offs]);
			axis->cur_velocity = EC_READ_S32(&pd[axis->cur_velocity_offs]);

			/* Set PINs values */
			*axis->cur_position_pin = (double)axis->current_position / POSITION_SCALE;
			*axis->cur_torque_pin = (int32_t)axis->cur_torque;
			*axis->position_err_pin = (double)axis->position_err / POSITION_SCALE;
			*axis->cur_velocity_pin = (double)axis->cur_velocity / VELOCITY_SCALE;

			*axis->is_homed_pin = axis->sw & SW_REFERENCE_DONE;
		} else {
			if(*axis->in_mode_csp_pin) *axis->cur_position_pin = *axis->target_position_pin;
			if(*axis->in_mode_csv_pin) *axis->cur_velocity_pin = *axis->target_velocity_pin;
			*axis->is_homed_pin = 1;
		}

		if (axis->sw & SW_FAULT) {
			axis_set_state(axis, sm_fault);
		}

		for (extra_var_i = 0; extra_var_i < 4; ++extra_var_i) {
			if(!debug) *axis->extra_tx_vars_pins[extra_var_i] = *(float *)&pd[axis->extra_tx_vars_offs[extra_var_i]];
		}

		axis->state(axis, pd, true);


	}
}

void lcec_tsd80e_ui_update(struct lcec_slave *slave, long period)
{
	int i = 0;
	int j;
	lcec_tsd80e_data_t *hal_data;
	lcec_tsd80e_axis_t *axis;
	hal_data = slave->hal_data;
	for (i = 0; i < LCEC_TSD80E_AXIS; i++) {
		axis = &hal_data->axis[i];
		for (j = 0; j < 16; j++) {
			*axis->sw_bits[j] = axis->sw & (1 << j);
		}

		for (j = 0; j < 16; j++) {
			*axis->cw_bits[j] = axis->cw & (1 << j);
		}
	}
}

int lcec_tsd80e_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	lcec_master_t *master = slave->master;
	lcec_tsd80e_data_t *hal_data;
	int err;
	int i;
	unsigned offset;
	lcec_tsd80e_axis_t *axis;
	char name[HAL_NAME_LEN + 1];
	int extra_var_i = 0;
	int bit_i = 0;

	debug = master->debug;

	/* Enable all messages */
	rtapi_set_msg_level(RTAPI_MSG_WARN);

	/* initialize callbacks */
	slave->proc_write = lcec_tsd80e_write;
	slave->proc_read = lcec_tsd80e_read;

	/* alloc hal memory */
	if ((hal_data = hal_malloc(__core_hal_user, sizeof(lcec_tsd80e_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
		return -EIO;
	}
	memset(hal_data, 0, sizeof(lcec_tsd80e_data_t));
	slave->hal_data = hal_data;

	/* initializer sync info */
	slave->sync_info = lcec_tsd80e_syncs;

	for (i = 0; i < LCEC_TSD80E_AXIS; i++) {
		axis = &hal_data->axis[i];

		/* the next axis data is offset by 0x800 for each */
		offset = i << 11;

		// LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid,
		// 0x23F0 + offset, 0x00, &axis->act_current_offs,
		//               &axis->act_current_bitp);

		if(!debug) {
			/* initialize PDO entries     position      vend.id     prod.code index
		   sindx offset                       bit pos TX */
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6040 + offset, 0x00,
					&axis->ctrl_word_offs, &axis->ctrl_word_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + offset, 0x00, &axis->op_mode_offs,
					&axis->op_mode_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x607a + offset, 0x00,
					&axis->pos_target_offs, &axis->pos_target_bitp);

			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60ff + offset, 0x00,
					&axis->vel_target_offs, &axis->vel_target_bitp);
			/* RX */
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6041 + offset, 0x00,
					&axis->status_word_offs, &axis->status_word_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6061 + offset, 0x00,
					&axis->op_mode_display_offs, &axis->op_mode_display_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6064 + offset, 0x00,
					&axis->cur_position_offs, &axis->cur_position_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60f4 + offset, 0x00,
					&axis->position_err_offs, &axis->position_err_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x606c + offset, 0x00,
					&axis->cur_velocity_offs, &axis->cur_velocity_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6077 + offset, 0x00,
					&axis->cur_torque_offs, &axis->cur_torque_bitp);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x603f + offset, 0x00, &axis->err_code_offs,
					&axis->err_code_bitp);
		}

		/* initialize extra Tx variables (4 per axis) */
		for (extra_var_i = 0; extra_var_i < 4; ++extra_var_i) {
			if(!debug) {
				LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid,
						slave_tsd80e_txpdo_extra[extra_var_i * 2].index + offset,
						slave_tsd80e_txpdo_extra[extra_var_i * 2].subindex, &axis->extra_tx_vars_offs[extra_var_i],
						&axis->extra_tx_vars_bitp[extra_var_i]);
			}
			lcec_pin_newf(HAL_FLOAT, HAL_OUT, (void **)&axis->extra_tx_vars_pins[extra_var_i], "%s.%s.%s.extra-var%d-%d",
						  lcec_module_name, master->name, slave->name, extra_var_i, i);
		}

		/* initialize extra Rx variables (2 per axis) */
		/* @Hack Don't use digout0s*/
		for (extra_var_i = 1; extra_var_i < 2; ++extra_var_i) {
			if(!debug) {
				LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid,
						slave_tsd80e_rxpdo_extra[extra_var_i * 2].index + offset,
						slave_tsd80e_rxpdo_extra[extra_var_i * 2].subindex, &axis->extra_rx_vars_offs[extra_var_i],
						&axis->extra_rx_vars_bitp[extra_var_i]);
			}
		}

		for(extra_var_i = 1; extra_var_i < 2; ++extra_var_i) {
			lcec_pin_newf(HAL_S32, HAL_IN, (void **)&axis->extra_rx_vars_pins[extra_var_i],
						  "%s.%s.%s.extra-rx-var%d-%d", lcec_module_name, master->name, slave->name, extra_var_i, i);
		}

		/* export pins */
		if ((err = lcec_pin_newf_list(axis, slave_pins, lcec_module_name, master->name, slave->name, i)) != 0) {
			return err;
		}

		for (bit_i = 0; bit_i < 16; bit_i++) {
			lcec_pin_newf(HAL_BIT, HAL_OUT, (void **)&axis->sw_bits[bit_i], "%s.%s.%s.sw-bit%d-%d", lcec_module_name,
						  master->name, slave->name, bit_i, i);

			lcec_pin_newf(HAL_BIT, HAL_OUT, (void **)&axis->cw_bits[bit_i], "%s.%s.%s.cw-bit%d-%d", lcec_module_name,
						  master->name, slave->name, bit_i, i);
		}

		/* initialization of drive control variables */
		axis_set_state(axis, sm_init);
		axis->current_position = 0;
		axis->target_pos = 0;
		axis->target_vel = 0;
		axis->cur_torque = 0;
		axis->cur_velocity = 0;
		axis->sw = 0;
		axis->target_mode = CIA402_OP_MODE_NONE;
	}

#if 0
	// lcec_param_newf_list(hal_data, slave_params, lcec_module_name,
	// master->name, slave->name, 0);
#endif

	lcec_pin_newf(HAL_U32, HAL_OUT, (void **)&hal_data->period_pin, "%s.%s.%s.period", lcec_module_name, master->name,
				  slave->name);
	rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s.%s.ui_update", lcec_module_name, master->name, slave->name);
	hal_export_funct(__core_hal_user, name, (hal_func)lcec_tsd80e_ui_update, slave, 1, 0, comp_id);

#if 0
	uint32_t serial_number;
	// lcec_read_sdo(slave, 0x1018, 0x4, (uint8_t *)&serial_number,
	// sizeof(serial_number));
	// rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "serial number: %d\n",
	// serial_number);
#endif
	return 0;
}
