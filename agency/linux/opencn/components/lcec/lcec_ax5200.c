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

#include <linux/slab.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include "lcec_priv.h"
#include "lcec_ax5200.h"

#define LCEC_IDN(type, set, block) (type | ((set & 0x07) << 12) | (block & 0x0fff))

#define IDN_TYPE_S(set, block) LCEC_IDN(0x0000, set, block)
#define IDN_TYPE_P(set, block) LCEC_IDN(0x8000, set, block)

#define LCEC_AX5X00_DEBUG_RESOLUTION  1048576
#define LCEC_AX5X00_DEFAULT_SCALE	  1.0


static rtdm_task_t idn_task;
static bool idn_read_done = false;

/* Master 0, Slave 0, "AX5203-0000-0210"
 * Vendor ID:       0x00000002
 * Product code:    0x14536012
 * Revision number: 0x00d20000
 */

static ec_pdo_entry_info_t ax5200_mdt[] = {
	{0x0086, 0x00, 16}, /* Master control word */
	{0x002f, 0x00, 32}, /* Position command value */
	{0x0086, 0x01, 16}, /* Master control word */
	{0x002f, 0x01, 32}, /* Position command value */
};

static ec_pdo_entry_info_t ax5200_at[] = {
	{0x0087, 0x00, 16}, /* Drive status word */
	{0x0033, 0x00, 32}, /* Position feedback 1 value */
	{0x0087, 0x01, 16}, /* Drive status word */
	{0x0033, 0x01, 32}, /* Position feedback 1 value */
};

static ec_pdo_info_t ax5200_pdos[] = {
    {0x0018, 2, ax5200_mdt + 0}, /* MDT 1 */
    {0x1018, 2, ax5200_mdt + 2}, /* MDT 2 */
    {0x0010, 2, ax5200_at + 0}, /* AT 1 */
    {0x1010, 2, ax5200_at + 2}, /* AT 2 */
};

static ec_sync_info_t ax5200_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT,  0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 2, ax5200_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT,  2, ax5200_pdos + 2, EC_WD_DISABLE},
    {4, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {5, EC_DIR_INPUT,  0, NULL, EC_WD_DISABLE},
    {0xff}
};

typedef struct {
	/* pins */
	hal_bit_t   *enable_pin;
	hal_bit_t   *enabled_pin;
	hal_bit_t   *fault_pin;
	hal_float_t *target_position_pin;
	hal_u32_t   *status_pin;
	hal_float_t *pos_feedback_pin;

	/* parameters */
	hal_float_t scale_param;
	hal_u32_t   position_resolution_param;

	/* PDOs */
	unsigned int ctrl_word;
	unsigned int position_command;
	unsigned int status_word;
	unsigned int pos_feedback;
} lcec_ax5x00_axis_t;

typedef struct {
	lcec_ax5x00_axis_t *axis;
	unsigned axis_nr;
} lcec_ax5x00_data_t;


static const lcec_pindesc_t slave_pins[] = {
	{ HAL_BIT,   HAL_IN,  offsetof(lcec_ax5x00_axis_t, enable_pin),  "%s.%s.%s.axis%d.enable" },
	{ HAL_BIT,   HAL_OUT, offsetof(lcec_ax5x00_axis_t, enabled_pin), "%s.%s.%s.axis%d.enabled" },
	{ HAL_BIT,   HAL_OUT, offsetof(lcec_ax5x00_axis_t, fault_pin),   "%s.%s.%s.axis%d.fault" },
	{ HAL_FLOAT, HAL_IN,  offsetof(lcec_ax5x00_axis_t, target_position_pin), "%s.%s.%s.axis%d.target-position" },
	{ HAL_U32,   HAL_OUT, offsetof(lcec_ax5x00_axis_t, status_pin), "%s.%s.%s.axis%d.status" },
	{ HAL_FLOAT, HAL_OUT, offsetof(lcec_ax5x00_axis_t, pos_feedback_pin), "%s.%s.%s.axis%d.position-feedback" },

	{ HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
	{ HAL_FLOAT, HAL_RW, offsetof(lcec_ax5x00_axis_t, scale_param), "%s.%s.%s.axis%d.scale" },
	{ HAL_U32, HAL_RO, offsetof(lcec_ax5x00_axis_t, position_resolution_param), "%s.%s.%s.axis%d.pos-resolution" },
	{ HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/* IDNs read task (Sercos Parameters)
 *   
 * This task read IDNs at AX5000 initialization. list of read IDNs:
 *     * S-0-0079 / Position resolution
 *
 * NB: lcec_read_idn blocks until the request was processed. This task should
 *     be exectued in a low priority thread  
 */
static void idn_task_fn(void *args)
{
	struct lcec_slave *slave = (struct lcec_slave *)args;
	lcec_ax5x00_data_t *hal_data = (lcec_ax5x00_data_t *)slave->hal_data;
	lcec_ax5x00_axis_t *axis;
	int i;
	uint8_t idn_buf[4];

	/* Wait for the drive to be at least in SAFEOP state
	    S-0-0079 IDN can be changed in PREOP state only
	*/
	while (slave->state.al_state <= EC_AL_STATE_PREOP) {
		rtdm_task_sleep(MILLISECS(10));
	}

	for (i = 0; i < hal_data->axis_nr; i++) {
		axis = &hal_data->axis[i];
		lcec_read_idn(slave, i, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 79), idn_buf, 4);
		axis->position_resolution_param = EC_READ_U32(idn_buf);
	}

	idn_read_done = true;
}

void lcec_ax5x00_read(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_ax5x00_data_t *hal_data = (lcec_ax5x00_data_t *)slave->hal_data;
	lcec_ax5x00_axis_t *axis;
	uint16_t status;
	int pos_feedback;
	int i;
	int debug = master->debug;

	/* wait for slave to be operational */
	if ((!slave->state.operational) || (!idn_read_done))  {
		for (i = 0; i < hal_data->axis_nr; i++) {
			axis = &hal_data->axis[i];

			*(axis->fault_pin) = 1;
			*(axis->enabled_pin) = 0;
		}
		return;
	}

	for (i = 0; i < hal_data->axis_nr; i++) {
		axis = &hal_data->axis[i];

		if (debug) {
			*(axis->fault_pin) = 0;
			*(axis->enabled_pin) = 1;

			*(axis->pos_feedback_pin) = *(axis->target_position_pin);

		} else {

			status = EC_READ_U16(&pd[axis->status_word]);
			*(axis->status_pin) = status;

			pos_feedback = EC_READ_S32(&pd[axis->pos_feedback]);
			*axis->pos_feedback_pin = ((double)pos_feedback / (double)(axis->position_resolution_param)) * axis->scale_param;

			*axis->fault_pin = (status >> 13) & 1;

			*axis->enabled_pin = (((status >> 14) & 3) == 3);
		}
	}
}

static void lcec_ax5x00_write(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data; /* process data */
	lcec_ax5x00_data_t *hal_data = (lcec_ax5x00_data_t *)slave->hal_data;
	lcec_ax5x00_axis_t *axis;
	int i;
	int position_command;
	uint16_t ctrl;
	int debug = master->debug;

	if (debug)
		return;

	for (i = 0; i < hal_data->axis_nr; i++) {
		axis = &hal_data->axis[i];

		/* Enable - Disable drive */
		ctrl = EC_READ_U16(&pd[axis->ctrl_word]);
		if (*axis->enable_pin) {
			ctrl |= (1 << 15); /* Drive ON */
			ctrl |= (1 << 14); /* Enable */
			ctrl |= (1 << 13); /* Halt/Restart */
		} else {
			ctrl &= ~(1 << 15); /* Drive ON */
			ctrl &= ~(1 << 14); /* Enable */
			ctrl &= ~(1 << 13); /* Halt/Restart */
		}
		ctrl ^= 1 << 10; /* Sync */
		EC_WRITE_U16(&pd[axis->ctrl_word], ctrl);

		/* Set target position */
		position_command = (*(axis->target_position_pin) * (double)(axis->position_resolution_param)) / axis->scale_param;

		EC_WRITE_S32(&pd[axis->position_command], position_command);
	}
}


int lcec_ax5x00_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, unsigned axis_nr)
{
	lcec_master_t *master = slave->master;
	lcec_ax5x00_data_t *hal_data;
	lcec_ax5x00_axis_t *axis;
	int i;
	int err;
	int debug = master->debug;
	bool read_idn = false;

	/* initialize callbacks */
	slave->proc_read  = lcec_ax5x00_read;
	slave->proc_write = lcec_ax5x00_write;

	/* alloc hal memory */
	if ((hal_data = hal_malloc(__core_hal_user,  sizeof(lcec_ax5x00_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
		return -EIO;
	}
	memset(hal_data, 0, sizeof(lcec_ax5x00_data_t));

	hal_data->axis_nr = axis_nr;
	slave->hal_data = hal_data;
	axis = (lcec_ax5x00_axis_t *)hal_malloc(__core_hal_user, sizeof(lcec_ax5x00_axis_t)*axis_nr);
	hal_data->axis = axis;

	for (i = 0; i < axis_nr; i++) {
		axis = &hal_data->axis[i];

		if (!debug) {
			/* initialize PDO entries     position      vend.id     prod.code   index   sindx  offset                   bit pos */
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0086, 0x00 + i,  &axis->ctrl_word,        NULL);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x002F, 0x00 + i,  &axis->position_command, NULL);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0087, 0x00 + i,  &axis->status_word,      NULL);
			LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0033, 0x00 + i,  &axis->pos_feedback,     NULL);
		}

		/* export pins */
		if ((err = lcec_pin_newf_list(axis, slave_pins, lcec_module_name, master->name, slave->name, i)) != 0)
			return err;

		/* export params */
		if ((err = lcec_param_newf_list(axis, slave_params, lcec_module_name, master->name, slave->name, i)) != 0)
			return err;

		/* Default scale value */
		axis->scale_param = LCEC_AX5X00_DEFAULT_SCALE;

		if (debug)
			axis->position_resolution_param = LCEC_AX5X00_DEBUG_RESOLUTION;
		else
			read_idn = true;
	}

	if (read_idn)
		rtdm_task_init(&idn_task, "idn_task", idn_task_fn, slave, RTDM_TASK_LOWEST_PRIORITY, 0);

	return 0;
}


int lcec_ax5200_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	int err;

	/* initializer sync info */
	slave->sync_info = ax5200_syncs;

	err = lcec_ax5x00_init(slave, pdo_entry_regs, LCEC_AX5200_AXIS);

	return err;
}
