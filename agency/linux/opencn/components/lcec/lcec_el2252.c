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

#include <opencn/uapi/lcec.h>

#include "lcec_priv.h"
#include "lcec_el2252.h"

/* Master 0, Slave 2, "EL2252"
 * Vendor ID:       0x00000002
 * Product code:    0x08cc3052
 * Revision number: 0x00120000
 */


ec_pdo_entry_info_t el2252_entries[] = {
	{0x1d09, 0x81, 8}, /* Activate */
	{0x1d09, 0x90, 64}, /* StartTime */
	{0x7000, 0x01, 1}, /* Output */
	{0x7000, 0x02, 1}, /* TriState */
	{0x7010, 0x01, 1}, /* Output */
	{0x7010, 0x02, 1}, /* TriState */
	{0x0000, 0x00, 4},
};

ec_pdo_info_t el2252_pdos[] = {
	{0x1602, 1, el2252_entries + 0}, /* DC Sync Activate */
	{0x1603, 1, el2252_entries + 1}, /* DC Sync Start */
	{0x1600, 2, el2252_entries + 2}, /* Channel 1 */
	{0x1601, 3, el2252_entries + 4}, /* Channel 2 */
	{0x1604, 0, NULL}, /* Reserved */
};

ec_sync_info_t el2252_syncs[] = {
	{0, EC_DIR_INPUT,  1, el2252_pdos + 0, EC_WD_DISABLE},
	{1, EC_DIR_INPUT,  1, el2252_pdos + 1, EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 3, el2252_pdos + 2, EC_WD_ENABLE},
	{3, EC_DIR_INPUT,  0, NULL, EC_WD_DISABLE},
	{4, EC_DIR_INPUT,  0, NULL, EC_WD_DISABLE},
	{0xff}
};


enum TRIGGER_STATE {
	IDLE,
	SET_TRIGGER,
	ACTIVATE_TRIGGER
};

/** \brief complete data structure for EL2252 */
typedef struct {
	/* data exposed as PIN to */
//    rtapi_u64 *startTime;            //< have to be cleanup - use hal type
	hal_bit_t *out[2];
	hal_bit_t *tristate[2];
	/* Internal data (PDOs) not exposed */
//    rtapi_u8  activate;             //< have to be cleanup - use hal type
	// OffSets and BitPositions used to access data in EC PDOs
		// Stores PDO entry's (byte-)offset in the process data.
	unsigned int activate_offs;
	unsigned int start_time_offs;
	unsigned int out_offs[2];
	unsigned int tristate_offs[2];

	// Store a bit position (0-7) within the above offset
	unsigned int out_bitp[2];
	unsigned int tristate_bitp[2];
} lcec_el2252_data_t;

static const lcec_pindesc_t slave_pins[] = {
	{ HAL_BIT, HAL_IN, offsetof(lcec_el2252_data_t, out[0]),      "%s.%s.%s.dout-0"},
	{ HAL_BIT, HAL_IN, offsetof(lcec_el2252_data_t, out[1]),      "%s.%s.%s.dout-1"},
	{ HAL_BIT, HAL_IN, offsetof(lcec_el2252_data_t, tristate[0]), "%s.%s.%s.tristate-0"},
	{ HAL_BIT, HAL_IN, offsetof(lcec_el2252_data_t, tristate[1]), "%s.%s.%s.tristate-1"},
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/** \brief callback for periodic IO data access*/
static void lcec_el2252_write(struct lcec_slave *slave, long period)
{
	static unsigned blink = 0;
#if 0
	static unsigned counter = 7;
#endif

	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_el2252_data_t *hal_data = (lcec_el2252_data_t *) slave->hal_data;
	static enum TRIGGER_STATE trigger = IDLE;
	uint64_t trigger_time;

	switch(trigger) {
	case IDLE:
#if 0
		if (counter) {
			counter--;
		} else {
			trigger = SET_TRIGGER;
			counter = 7; // n + 3ms = level duration
		}
#else
		trigger = SET_TRIGGER;
#endif
		break;
	case ACTIVATE_TRIGGER:
		/* Step 2: Activate the trigger */
		EC_WRITE_U8(&pd[hal_data->activate_offs], 3);
		trigger = IDLE;

		break;
	case SET_TRIGGER:
		trigger_time = rtapi_get_time() + (200000);
		blink = !blink;

		/* TODO
		   - add tristate
		   - Toggle_time pin
		   - Testing !!
		*/
		EC_WRITE_U8(&pd[hal_data->activate_offs], 0);
		EC_WRITE_U64(&pd[hal_data->start_time_offs], trigger_time);
		EC_WRITE_BIT(&pd[hal_data->out_offs[0]], hal_data->out_bitp[0], blink);
		EC_WRITE_BIT(&pd[hal_data->out_offs[1]], hal_data->out_bitp[1], !blink);

		trigger = ACTIVATE_TRIGGER;
		break;
	}
}

int lcec_el2252_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	int err;

	lcec_el2252_data_t *hal_data;
	lcec_master_t *master = slave->master;

	/* Enable all messages */
	rtapi_set_msg_level(RTAPI_MSG_ALL);

	slave->proc_write = lcec_el2252_write;

	/* alloc hal memory */
	if ((hal_data = hal_malloc(__core_hal_user, sizeof(lcec_el2252_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
		return -EIO;
	}
	memset(hal_data, 0, sizeof(lcec_el2252_data_t));
	slave->hal_data = hal_data;

	/* initializer sync info */
	slave->sync_info = el2252_syncs;

	rtapi_print_msg(RTAPI_MSG_ALL, "EL2252 - index: %d, vid: %d, pid: %d\n", slave->index, slave->vid, slave->pid);

	/* initialize PDO entries     position      vend.id     prod.code   index         sindx  offset               bit pos */
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1d09, 0x81, &hal_data->activate_offs,     NULL);
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1d09, 0x90, &hal_data->start_time_offs,   NULL);
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->out_offs[0],       &hal_data->out_bitp[0]);
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x02, &hal_data->tristate_offs[0],  &hal_data->tristate_bitp[0]);
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01, &hal_data->out_offs[1],       &hal_data->out_bitp[1]);
	LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x02, &hal_data->tristate_offs[1],  &hal_data->tristate_bitp[1]);


	/* export pins */
	if ((err = lcec_pin_newf_list(hal_data, slave_pins, lcec_module_name, master->name, slave->name, 0)) != 0) {
		return err;
	}

	return 0;
}
