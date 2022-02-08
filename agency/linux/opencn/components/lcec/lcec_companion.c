/********************************************************************
 *  Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
 *  Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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
#include "lcec_companion.h"

#define COMP_GPIO_NR		4
#define COMP_PWM_NR			2

#define COMP_PWM_DEFAULT_CLK_DIV		16

#warning COMP_PWM_VAL_MAX value should be shared with companion driver
#define COMP_PWM_VAL_MAX 	200

#define PDO_IN  0x6
#define PDO_OUT 0x5

/* Master 0, Slave 0, "EasyCAT 32+32 rev 1"
 * Vendor ID:       0x0000079a
 * Product code:    0x00defede
 * Revision number: 0x00005a01
 */

ec_pdo_entry_info_t companion_out_entries[] = {
    {0x0005, 0x01, 8}, /* output GPIOs */
    {0x0005, 0x02, 8}, /* Reserved (GPIOs extension) */
    {0x0005, 0x03, 8}, /* duty_cycle_offs 0 */
    {0x0005, 0x04, 8}, /* duty_cycle_offs 1 */
    {0x0005, 0x05, 8}, /* Byte4 */
    {0x0005, 0x06, 8}, /* Byte5 */
    {0x0005, 0x07, 8}, /* Byte6 */
    {0x0005, 0x08, 8}, /* Byte7 */
    {0x0005, 0x09, 8}, /* Byte8 */
    {0x0005, 0x0a, 8}, /* Byte9 */
    {0x0005, 0x0b, 8}, /* Byte10 */
    {0x0005, 0x0c, 8}, /* Byte11 */
    {0x0005, 0x0d, 8}, /* Byte12 */
    {0x0005, 0x0e, 8}, /* Byte13 */
    {0x0005, 0x0f, 8}, /* Byte14 */
    {0x0005, 0x10, 8}, /* Byte15 */
    {0x0005, 0x11, 8}, /* Byte16 */
    {0x0005, 0x12, 8}, /* Byte17 */
    {0x0005, 0x13, 8}, /* Byte18 */
    {0x0005, 0x14, 8}, /* Byte19 */
    {0x0005, 0x15, 8}, /* Byte20 */
    {0x0005, 0x16, 8}, /* Byte21 */
    {0x0005, 0x17, 8}, /* Byte22 */
    {0x0005, 0x18, 8}, /* Byte23 */
    {0x0005, 0x19, 8}, /* Byte24 */
    {0x0005, 0x1a, 8}, /* Byte25 */
    {0x0005, 0x1b, 8}, /* Byte26 */
    {0x0005, 0x1c, 8}, /* Byte27 */
    {0x0005, 0x1d, 8}, /* Byte28 */
    {0x0005, 0x1e, 8}, /* Byte29 */
    {0x0005, 0x1f, 8}, /* Byte30 */
    {0x0005, 0x20, 8}, /* Byte31 */
};


ec_pdo_entry_info_t companion_in_entries[] = {
    {0x0006, 0x01, 8}, /* Input GPIOs */
    {0x0006, 0x02, 8}, /* Reserved (GPIOs extension) */
    {0x0006, 0x03, 8}, /* Byte2 */
    {0x0006, 0x04, 8}, /* Byte3 */
    {0x0006, 0x05, 8}, /* Byte4 */
    {0x0006, 0x06, 8}, /* Byte5 */
    {0x0006, 0x07, 8}, /* Byte6 */
    {0x0006, 0x08, 8}, /* Byte7 */
    {0x0006, 0x09, 8}, /* Byte8 */
    {0x0006, 0x0a, 8}, /* Byte9 */
    {0x0006, 0x0b, 8}, /* Byte10 */
    {0x0006, 0x0c, 8}, /* Byte11 */
    {0x0006, 0x0d, 8}, /* Byte12 */
    {0x0006, 0x0e, 8}, /* Byte13 */
    {0x0006, 0x0f, 8}, /* Byte14 */
    {0x0006, 0x10, 8}, /* Byte15 */
    {0x0006, 0x11, 8}, /* Byte16 */
    {0x0006, 0x12, 8}, /* Byte17 */
    {0x0006, 0x13, 8}, /* Byte18 */
    {0x0006, 0x14, 8}, /* Byte19 */
    {0x0006, 0x15, 8}, /* Byte20 */
    {0x0006, 0x16, 8}, /* Byte21 */
    {0x0006, 0x17, 8}, /* Byte22 */
    {0x0006, 0x18, 8}, /* Byte23 */
    {0x0006, 0x19, 8}, /* Byte24 */
    {0x0006, 0x1a, 8}, /* Byte25 */
    {0x0006, 0x1b, 8}, /* Byte26 */
    {0x0006, 0x1c, 8}, /* Byte27 */
    {0x0006, 0x1d, 8}, /* Byte28 */
    {0x0006, 0x1e, 8}, /* Byte29 */
    {0x0006, 0x1f, 8}, /* Byte30 */
    {0x0006, 0x20, 8}, /* Byte31 */
};

ec_pdo_info_t companion_pdos[] = {
    {0x1600, 32, companion_out_entries}, /* Outputs */
    {0x1a00, 32, companion_in_entries}, /* Inputs */
};

ec_sync_info_t companion_syncs[] = {
    {0, EC_DIR_OUTPUT, 1, companion_pdos + 0, EC_WD_ENABLE},
    {1, EC_DIR_INPUT,  1, companion_pdos + 1, EC_WD_DISABLE},
    {0xff}
};

/** \brief data structure of one channel of the device */
typedef struct {
	/* GPIOs stuff */
	hal_bit_t *out_bit[4];
	hal_bit_t *in_bit[4];
	unsigned int out_offs;
	unsigned int in_offs;

	/* PWM stuff */
	hal_u32_t pwm_clk_divisor;
	hal_u32_t *duty_cycle[2];
	unsigned int duty_cycle_offs[2];
} lcec_companion_data_t;

static const lcec_pindesc_t slave_pins[] = {
	/* GPIOs pins */
	{ HAL_BIT, HAL_OUT, offsetof(lcec_companion_data_t, in_bit[0]),  "%s.%s.%s.in-0"},
	{ HAL_BIT, HAL_OUT, offsetof(lcec_companion_data_t, in_bit[1]),  "%s.%s.%s.in-1"},
	{ HAL_BIT, HAL_OUT, offsetof(lcec_companion_data_t, in_bit[2]),  "%s.%s.%s.in-2"},
	{ HAL_BIT, HAL_OUT, offsetof(lcec_companion_data_t, in_bit[3]),  "%s.%s.%s.in-3"},
	{ HAL_BIT, HAL_IN,  offsetof(lcec_companion_data_t, out_bit[0]), "%s.%s.%s.out-0"},
	{ HAL_BIT, HAL_IN,  offsetof(lcec_companion_data_t, out_bit[1]), "%s.%s.%s.out-1"},
	{ HAL_BIT, HAL_IN,  offsetof(lcec_companion_data_t, out_bit[2]), "%s.%s.%s.out-2"},
	{ HAL_BIT, HAL_IN,  offsetof(lcec_companion_data_t, out_bit[3]), "%s.%s.%s.out-3"},

	/* PWM pins */
	{ HAL_U32, HAL_IN, offsetof(lcec_companion_data_t, duty_cycle[0]), "%s.%s.%s.pwm-duty-cycle-0"},
	{ HAL_U32, HAL_IN, offsetof(lcec_companion_data_t, duty_cycle[1]), "%s.%s.%s.pwm-duty-cycle-1"},

	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};


static const lcec_pindesc_t slave_params[] = {
	/* PWM parameter */
	{HAL_U32, HAL_RW, offsetof(lcec_companion_data_t, pwm_clk_divisor), "%s.%s.%s.pwm-clk-divisor"},

	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};


void lcec_companion_read(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_companion_data_t *hal_data = (lcec_companion_data_t *) slave->hal_data;
	int i;
	uint8_t in_val;
	int debug = master->debug;

	if (!debug) {
		in_val = EC_READ_U8(&pd[hal_data->in_offs]);

		for (i = 0; i< COMP_GPIO_NR; i++)
			*hal_data->in_bit[i] = (in_val & 1 << i) >> i;
	}
}

/** \brief callback for periodic IO data access*/
static void lcec_companion_write(struct lcec_slave *slave, long period)
{
	lcec_master_t *master = slave->master;
	uint8_t *pd = master->process_data;
	lcec_companion_data_t *hal_data = (lcec_companion_data_t *) slave->hal_data;
	int i;
	uint8_t out_val = 0;
	uint8_t pwm_val;
	int debug = master->debug;

	/* Write the GPIOS OUT pin */
	if (!debug) {

		for (i = 0; i < COMP_GPIO_NR; i++)
			out_val += *hal_data->out_bit[i] << i;
		EC_WRITE_U8(&pd[hal_data->out_offs], out_val);

		/* Write the PWM duty cycle */
		for (i = 0; i < COMP_PWM_NR; i++) {
			pwm_val = (uint8_t) *(hal_data->duty_cycle[i]);
			if (pwm_val > COMP_PWM_VAL_MAX)
				pwm_val = COMP_PWM_VAL_MAX;

			EC_WRITE_U8(&pd[hal_data->duty_cycle_offs[i]], pwm_val);
		}
	} else {
		/* Debug mode - PINs loop back */
		for (i = 0; i < COMP_GPIO_NR; i++)
			*hal_data->in_bit[i] = *hal_data->out_bit[i];
	}
}

int lcec_companion_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	lcec_master_t *master = slave->master;
	lcec_companion_data_t *hal_data;
	int err;
	int debug = master->debug;


	/* initialize callbacks */
	slave->proc_read = lcec_companion_read;
	slave->proc_write = lcec_companion_write;

	/* alloc hal memory */
	if ((hal_data = hal_malloc(__core_hal_user, sizeof(lcec_companion_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
		return -EIO;
	}
	memset(hal_data, 0, sizeof(lcec_companion_data_t));
	slave->hal_data = hal_data;

	/* initializer sync info */
	slave->sync_info = companion_syncs;

	if (!debug) {
		/* initialize PDO entries     position      vend.id     prod.code   index   sindx  	offset                 bit pos */
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x5,     0x1,   &hal_data->out_offs,   NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6,     0x1,   &hal_data->in_offs,    NULL);

		/* PWM related PDOs *//* GPIO related PDOs */
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x5, 	 0x3,   &hal_data->duty_cycle_offs[0],   NULL);
		LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x5, 	 0x4, 	&hal_data->duty_cycle_offs[1],   NULL);
	}

	/* export pins */
	if ((err = lcec_pin_newf_list(hal_data, slave_pins, lcec_module_name, master->name, slave->name, 0)) != 0) {
		return err;
	}

	/* Export parameters */
	if ((err = lcec_param_newf_list(hal_data, slave_params, lcec_module_name, master->name, slave->name, 0)) != 0)
			return err;

	/* Set default parameters values */
	hal_data->pwm_clk_divisor = COMP_PWM_DEFAULT_CLK_DIV;

	return 0;
}
