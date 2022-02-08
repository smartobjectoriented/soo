/*
 * Copyright (C) 2020 David Truan <david.truan@@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include "companion.h"

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#define DEBUG 1

#define USE_SI7021 0


#if USE_SI7021
/*
 * Both functions below are here for testing purpose only and should BE REMOVED
 * from the final code (as it won't be SI7021 specific).
 */
static int read_firmware(struct i2c_client *client) {

	struct i2c_msg xfer[2];

	uint8_t cmd[2];
	uint8_t rsp;

	cmd[0] = SI7021_RD_FW1_CMD;
	cmd[1] = SI7021_RD_FW2_CMD;
	
	/* Write CMD */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = cmd;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = &rsp;

	i2c_transfer(client->adapter, xfer, 2);

	DBG("FIRMWARE VERSION 0x%X\n", rsp);

	return 0;
}

int get_temperature(struct i2c_client *client) {
	uint16_t temp_raw;
	uint32_t temp_celsius;

	temp_raw = i2c_smbus_read_word_data(client, SI7021_MES_RD_TEMP_CMD);
	/* Invert LSB and MSB */
	temp_raw = ((temp_raw << 8) & 0xFF00) | ((temp_raw >> 8) & 0x00FF);

	DBG("Temp RAW: 0x%X\n", temp_raw);
	/* For testing purpose for now */
	temp_celsius = ((17572 * temp_raw) / 6553600) - 47;

	DBG("Temp Celsius approx: 0x%X\n", temp_celsius);

	return 0;
}
#endif

/* For now, it is more a SI7021 driver and should be perpherial agnostic.
   It should be possible by not using the I2C slave address retrieved from the DT. */
static int companion_i2c_probe(struct i2c_client *client)
{
	struct companion_status *companion;

	/* Init main companion struct */
	companion = container_of(client->dev.driver, struct companion_status,
					     i2cdrv.driver);
	companion->i2c_client = client;

	/* App specific part. Enable this part if you have the SI7021 connected. */
#if USE_SI7021	
	/* Reset */
	i2c_smbus_write_byte(client, SI7021_RESET_CMD);
	/* Wait for reset done. 0x3A correspond to the reset setting
	   of the User Register */
	while (i2c_smbus_read_byte_data(client, SI7021_RD_USER_CMD) != SI7021_USR_DEFAULT_VAL);

	read_firmware(client);
	get_temperature(client);
#endif	

	return 0;
}

static int companion_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id companion_i2c_id[] = {
	{ "companion-i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, companion_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id companion_i2c_of_match[] = {
	{ .compatible = "bcm,companion" },
	{ },
};
MODULE_DEVICE_TABLE(of, companion_i2c_of_match);
#endif

/* To be called from the companion core driver. It simulate the init function
   by calling i2c_add_driver(). */
int companion_register_i2c_driver(struct companion_status *companion) {
	struct i2c_driver *i2cdrv = &companion->i2cdrv;

	i2cdrv->probe_new = companion_i2c_probe;
	i2cdrv->remove = companion_i2c_remove;
	i2cdrv->driver.name = "companion-i2c";
	i2cdrv->driver.of_match_table = of_match_ptr(companion_i2c_of_match);
	i2cdrv->id_table = companion_i2c_id;
	return i2c_add_driver(i2cdrv);
}