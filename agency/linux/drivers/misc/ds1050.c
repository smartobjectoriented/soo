/*
 * Copyright (C) 2018 David Truan <david.truan@@heig-vd.ch>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

/* Control sequence on 4 bits */
#define DS1050_CONTROL_SEQ                  0x5
/* Position of the control sequence */
#define DS1050_CONTROL_SEQ_POS              0x3

/* 3 MSB modes for the write functions */
#define DS1050_WRITE_PWM_DUTY_CYCLE         0x0
#define DS1050_WRITE_PWM_DUTY_CYCLE_FULL    0x1
#define DS1050_WRITE_SHUTDOWN_MODE          0x6
#define DS1050_WRITE_RECALL_MODE            0x4

/* Position of the mode in the write byte (3 MSB) */
#define DS1050_WRITE_MODE_POS               0x5

/* The duty cycle is on 5 bits */
#define DS1050_MAX_DUTY_CYCLE               0x1F

/*
 * The address is defined by the A2, A1 and A0 input on the board
 * It actually is 000 (SOO.indoor v2)
 */
#define DS1050_ADDRESS 0x00

static struct i2c_client *client_global;

static bool motor_enabled;
static bool is_full_speed;

/* Wrapper for the read and write */

/**
 * Write the value [value] into the DS1050.
 * The mode is passed for more flexibility
 */
static int ds1050_write_value(struct i2c_client *client, uint8_t value, uint8_t mode) {
	/* Check if the client passed is set */
	if (client == NULL)
		return -1;

	/* Add the mode to the value written (3 MSB) */
	value |= mode << DS1050_WRITE_MODE_POS;

	return i2c_smbus_write_byte(client, value);
}

/**
 * Read a byte from the DS1050.
 */
static uint8_t ds1050_read_value(struct i2c_client *client) {
	/* Check if the client passed is set */
	if (client == NULL)
		return -1;

	return i2c_smbus_read_byte(client);
}

/* Exported functions to be able to drive the DS1050 from another kernel driver */

/**
 * Write a new duty cycle
 */
void ds1050_set_duty_cycle(uint8_t duty_cycle) {
	uint8_t duty_cycle_convert;

	/* Check if the duty cycle passed is between 0 and 100 */
	if (duty_cycle > 100 || duty_cycle < 0)
		return;

	is_full_speed = false;

	/* Convert the percentage into a 5 bits value */
	duty_cycle_convert = (uint8_t) ((DS1050_MAX_DUTY_CYCLE * duty_cycle) / 100);

	ds1050_write_value(client_global, duty_cycle_convert, DS1050_WRITE_PWM_DUTY_CYCLE);
}
EXPORT_SYMBOL(ds1050_set_duty_cycle);

/**
 * Set the duty cycle to 100%
 */
void ds1050_duty_cycle_full(void) {
	is_full_speed = true;

	ds1050_write_value(client_global, 0, DS1050_WRITE_PWM_DUTY_CYCLE_FULL);
}
EXPORT_SYMBOL(ds1050_duty_cycle_full);

/**
 * Place the DS1050 in shutdown mode
 */
void ds1050_shutdown(void) {
	motor_enabled = false;

	ds1050_write_value(client_global, 0, DS1050_WRITE_SHUTDOWN_MODE);
}
EXPORT_SYMBOL(ds1050_shutdown);

/**
 * Recall from the shutdown mode
 */
void ds1050_recall(void) {
	motor_enabled = true;

	ds1050_write_value(client_global, 0, DS1050_WRITE_RECALL_MODE);
}
EXPORT_SYMBOL(ds1050_recall);

/**
 * Return the current duty cycle in percents
 */
uint8_t ds1050_get_duty_cycle(void) {
	uint8_t duty_cycle;

	/*
	 * Needed because the DS1050 doesn't change the PWM value to 100 if the full speed
	 * mode is set, so it would read the value prior to the full speed command.
	 */
	if (is_full_speed)
		return 100;

	duty_cycle =  ds1050_read_value(client_global) & DS1050_MAX_DUTY_CYCLE;

	duty_cycle = (uint8_t) ((duty_cycle * 100) / DS1050_MAX_DUTY_CYCLE );

	return duty_cycle;
}
EXPORT_SYMBOL(ds1050_get_duty_cycle);

/**
 * Return whether the motor is enabled or disabled
 */
bool ds1050_get_motor_enabled(void) {
	return motor_enabled;
}
EXPORT_SYMBOL(ds1050_get_motor_enabled);

/* Sysfs layer */

/**
 * sysfs entry to change the PWM duty cycle. It takes a percentage and does an internal check
 * to know if it is between 0-100. If it is out of bounds it return -1.
 */
static ssize_t duty_cycle_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {
	int duty_cycle;

	/* Convert the string passed into an integer */
	if (kstrtoint(buf, 10, &duty_cycle))
		return -1;

	ds1050_set_duty_cycle(duty_cycle);

	return count;
}

/**
 * sysfs entry to read the PWM duty cycle in percentage.
 */
static ssize_t duty_cycle_show(struct device *dev, struct device_attribute *attr,
				char *buf) {
	uint8_t duty_cycle = ds1050_get_duty_cycle();

	return snprintf(buf, 6, "%3u\n", (uint32_t) duty_cycle);
}

/**
 * sysfs entry make the output go continuous.
 */
static ssize_t duty_cycle_full_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count) {
	ds1050_duty_cycle_full();

	return count;
}

/**
 * sysfs entry to make the DS1050 go into low power. The output goes to 0V.
 * Notice that you must use the recall mode in order to cancel the low power mode.
 */
static ssize_t shutdown_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {
	ds1050_shutdown();

	return count;
}

/**
 * sysfs entry for the recall mode. The recall mode is used to recover from the low power mode
 * caused by a shutdown mode. It makes the duty cycle return in the state before the shutdown.
 */
static ssize_t recall_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {
	ds1050_recall();

	return count;
}

/* Sysfs attributes declaration */
static DEVICE_ATTR_RW(duty_cycle);
static DEVICE_ATTR_WO(duty_cycle_full);
static DEVICE_ATTR_WO(shutdown);
static DEVICE_ATTR_WO(recall);

static struct attribute *ds1050_attributes[] = {
	&dev_attr_duty_cycle.attr,
	&dev_attr_duty_cycle_full.attr,
	&dev_attr_shutdown.attr,
	&dev_attr_recall.attr,
	NULL
};

static const struct attribute_group ds1050_attr_group = {
	.attrs = ds1050_attributes,
};

static int ds1050_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {
	int rc;

	/* Check if the I2C adapter support the SMBUS protocol */
	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "i2c bus does not support the ds1050\n");
		rc = -ENODEV;
		goto exit;
	}

	rc = sysfs_create_group(&client->dev.kobj, &ds1050_attr_group);
	if (rc)
		goto exit;

	/*
	 * We save the client here as global so we can retrieve it in the exported functions and we do not
	 * rely only on the sysfs layer.
	 */
	client_global = client;

	/* Place the DS1050 in shutdown mode. */
	ds1050_shutdown();
	ds1050_set_duty_cycle(0);

	printk(KERN_INFO "DS1050 PROBED\n");

	return 0;

	exit:
	return rc;
}

static int ds1050_remove(struct i2c_client *client) {
	sysfs_remove_group(&client->dev.kobj, &ds1050_attr_group);
	return 0;
}

static const struct i2c_device_id ds1050_id[] = {
	{ "maxim,ds1050", 0 },
	{  }
};

#ifdef CONFIG_OF
static const struct of_device_id ds1050_of_match[] = {
	{ .compatible = "maxim,ds1050", },
	{  }
};
MODULE_DEVICE_TABLE(of, ds1050_of_match);
#endif

MODULE_DEVICE_TABLE(i2c, ds1050_id);

static struct i2c_driver ds1050_driver = {
	.driver = {
		.name = "ds1050",
		.owner = THIS_MODULE,
		.of_match_table	= of_match_ptr(ds1050_of_match),
	},
	.probe = ds1050_probe,
	.remove = ds1050_remove,
	.id_table = ds1050_id,
};

module_i2c_driver(ds1050_driver);
