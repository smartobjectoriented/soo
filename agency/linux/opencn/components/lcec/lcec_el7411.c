/********************************************************************
 *  Copyright (C) 2021 Jean-Pierre Miceli Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include "lcec_priv.h"
#include "lcec_el7411.h"

/* Master 0, Slave 1, "EL7411"
 * Vendor ID:       0x00000002
 * Product code:    0x1cf33052
 * Revision number: 0x00120000
 */

ec_pdo_entry_info_t slave_1_pdo_entries[] = {
    {0x7010, 0x01, 16}, /* Control Word */
    {0x7010, 0x05, 32}, /* Target position */
    {0x6000, 0x11, 32}, /* Position */
    {0x6010, 0x01, 16}, /* Status Word */
    {0x6010, 0x06, 32}, /* Following error actual value */
};

ec_pdo_info_t slave_1_pdos[] = {
    {0x1600, 1, slave_1_pdo_entries + 0}, /* DRV RxPDO-Map Control Word */
    {0x1606, 1, slave_1_pdo_entries + 1}, /* DRV RxPDO-Map Target position */
    {0x1a00, 1, slave_1_pdo_entries + 2}, /* FB TxPDO-Map Position */
    {0x1a01, 1, slave_1_pdo_entries + 3}, /* DRV TxPDO-Map Status Word */
    {0x1a06, 1, slave_1_pdo_entries + 4}, /* DRV TxPDO-Map Following error actual value */
};

ec_sync_info_t el7411_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 2, slave_1_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 3, slave_1_pdos + 2, EC_WD_DISABLE},
    {0xff}
};

typedef struct {
	/* PINs */
	hal_bit_t   *enable_pin;
	hal_bit_t   *enabled_pin;
	hal_float_t *target_position_pin;
	hal_u32_t   *status_pin;
	hal_float_t *pos_feedback_pin;
	hal_bit_t   *status_ready_pin;
	hal_bit_t   *status_switched_on_pin;
	hal_bit_t   *status_operation_pin;
	hal_bit_t   *status_fault_pin;
	hal_bit_t   *status_disabled_pin;
	hal_bit_t   *status_warning_pin;

	/* parameters */
	hal_float_t scale_param;

	/* PDOs */
	unsigned int ctrl_word;
	unsigned int position_command;
	unsigned int pos_feedback;
	unsigned int status_word;
	unsigned int following_error;
} lcec_el7411_data_t;


static const lcec_pindesc_t slave_pins[] = {
	{ HAL_BIT,   HAL_IN,  offsetof(lcec_el7411_data_t, enable_pin),  "%s.%s.%s.enable" },
	{ HAL_FLOAT, HAL_IN,  offsetof(lcec_el7411_data_t, target_position_pin), "%s.%s.%s.target-position" },

	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, enabled_pin),           "%s.%s.%s.enabled" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_ready_pin),       "%s.%s.%s.status-ready" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_switched_on_pin), "%s.%s.%s.status-switched-on" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_operation_pin),   "%s.%s.%s.status-operation" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_fault_pin),       "%s.%s.%s.status-fault" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_disabled_pin),    "%s.%s.%s.status-disabled" },
	{ HAL_BIT, HAL_OUT, offsetof(lcec_el7411_data_t, status_warning_pin),     "%s.%s.%s.status-warning" },

	{ HAL_FLOAT, HAL_OUT, offsetof(lcec_el7411_data_t, pos_feedback_pin), "%s.%s.%s.position-feedback" },

	{ HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
	{ HAL_FLOAT, HAL_RW, offsetof(lcec_el7411_data_t, scale_param), "%s.%s.%s.scale" },
	{ HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static void lcec_el7411_read(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_el7411_data_t *hal_data = (lcec_el7411_data_t *)slave->hal_data;
	uint16_t status;
	int32_t pos_feedback;
	int debug = master->debug;

/*
--> status Word
Bit 0 : Ready to switch on
Bit 1 : Switched on
Bit 2 : Operation enabled
Bit 3 : Fault
Bit 4 : reserved
Bit 5 : reserved
Bit 6 : Switch on disabled
Bit 7 : Warning
Bit 8 + 9 : reserved
Bit 10: TxPDOToggle (selection/deselection via0x8010:01 [}141])
Bit 11 : Internal limit active
Bit 12 : (Target value ignored)
Bit 13 - 15 : reserved
*/
	if (debug) {

		*(hal_data->status_ready_pin)       = 1;
		*(hal_data->status_switched_on_pin) = 1;
		*(hal_data->status_operation_pin)   = 1;
		*(hal_data->status_fault_pin)       = 0;
		*(hal_data->status_disabled_pin)    = 0;
		*(hal_data->status_warning_pin)     = 0;

		*(hal_data->enabled_pin) = 1;

		*hal_data->pos_feedback_pin = *(hal_data->target_position_pin);

	} else {
		status = EC_READ_U16(&pd[hal_data->status_word]);

		*(hal_data->status_ready_pin)       = (status >> 0) & 0x01;
		*(hal_data->status_switched_on_pin) = (status >> 1) & 0x01;
		*(hal_data->status_operation_pin)   = (status >> 2) & 0x01;
		*(hal_data->status_fault_pin)       = (status >> 3) & 0x01;
		*(hal_data->status_disabled_pin)    = (status >> 6) & 0x01;
		*(hal_data->status_warning_pin)     = (status >> 7) & 0x01;

		*(hal_data->enabled_pin) = *(hal_data->status_ready_pin) &&
		                           *(hal_data->status_switched_on_pin) &&
		                           *(hal_data->status_operation_pin);


		pos_feedback = EC_READ_S32(&pd[hal_data->pos_feedback]);
		*hal_data->pos_feedback_pin = (double)pos_feedback / hal_data->scale_param;
	}
}

static void lcec_el7411_write(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data; /* process data */
	lcec_el7411_data_t *hal_data = (lcec_el7411_data_t *)slave->hal_data;
	int position_command;
	uint16_t ctrl;
	int debug = master->debug;

/* Control Word
0 : Switch on
1 : Enable voltage
2 : reserved
3 : Enable operation
4 - 6 : reserved
7 : Fault reset
8 - 15 : reserved
*/

	/* Enable - Disable drive */
	if (!debug) {
		ctrl = EC_READ_U16(&pd[hal_data->ctrl_word]);
		if (*(hal_data->enable_pin)) {
			if (*(hal_data->status_fault_pin)) {
				ctrl = 0x80;
			} else if (*(hal_data->status_disabled_pin)) {
				ctrl = 0x06;
			} else if (*(hal_data->status_ready_pin)) {
				ctrl = 0x07;
				if (*(hal_data->status_switched_on_pin))
					ctrl = 0x0f;
			}
		} else {
			if (*(hal_data->status_ready_pin)) {
				if (*(hal_data->status_switched_on_pin))
					ctrl = 0x07;
				else
					ctrl = 0x06;
			} else {
				ctrl = 0x00;
			}
		}
		EC_WRITE_U16(&pd[hal_data->ctrl_word], ctrl);

		/* Set target position */
		position_command = *(hal_data->target_position_pin) * hal_data->scale_param;
		EC_WRITE_S32(&pd[hal_data->position_command], position_command);
	}
}

int lcec_el7411_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	lcec_master_t *master = slave->master;
	lcec_el7411_data_t *hal_data;
	int err;
	int debug = master->debug;

	/* initialize callbacks */
	slave->proc_read  = lcec_el7411_read;
	slave->proc_write = lcec_el7411_write;

	/* alloc hal memory */
	if ((hal_data = hal_malloc(__core_hal_user, sizeof(lcec_el7411_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
		return -EIO;
	}
	memset(hal_data, 0, sizeof(lcec_el7411_data_t));
	slave->hal_data = hal_data;

	/* initializer sync info */
	slave->sync_info = el7411_syncs;

	if (!debug) {
		/* initialize PDO entries     position      vend.id     prod.code   index   sindx  offset             bit pos */
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01,  &hal_data->ctrl_word,  NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x05,  &hal_data->position_command,  NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x11,  &hal_data->pos_feedback,  NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x01,  &hal_data->status_word,  NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x06,  &hal_data->following_error,  NULL);
	}

	/* export pins */
	if ((err = lcec_pin_newf_list(hal_data, slave_pins, lcec_module_name, master->name, slave->name)) != 0)
		return err;

	/* export params */
	if ((err = lcec_param_newf_list(hal_data, slave_params, lcec_module_name, master->name, slave->name)) != 0)
		return err;

	/* Default scale value */
	hal_data->scale_param = 1.0;

	return 0;
}
