/*
 * Copyright (C) 2018 David Truan <david.truan@@heig-vd.ch>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/spi/spi.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

/* Message bits and fields */
#define DRV8703Q1_RW					7
#define DRV8703Q1_ADDRESS				3
#define DRV8703Q1_ADDRESS_MASK				0xf
#define DRV8703Q1_DATA_MASK				0xff

/* Registers and fields */
#define DRV8703Q1_FAULT_STATUS_REG			0
#define DRV8703Q1_VDS_GDF_STATUS_REG			1
#define DRV8703Q1_MAIN_CONTROL_REG			2
#define DRV8703Q1_MAIN_CONTROL_LOCK			3
#define DRV8703Q1_MAIN_CONTROL_LOCK_MASK		0x7
#define DRV8703Q1_MAIN_CONTROL_IN1_PH			2
#define DRV8703Q1_MAIN_CONTROL_IN2_EN			1
#define DRV8703Q1_MAIN_CONTROL_CLR_FLT			0
#define DRV8703Q1_IDRIVE_WD_CONTROL_REG			3
#define DRV8703Q1_IDRIVE_WD_CONTROL_TDEAD		6
#define DRV8703Q1_IDRIVE_WD_CONTROL_TDEAD_MASK		0x3
#define DRV8703Q1_IDRIVE_WD_CONTROL_WD_EN		5
#define DRV8703Q1_IDRIVE_WD_CONTROL_WD_DLY		3
#define DRV8703Q1_IDRIVE_WD_CONTROL_WD_DLY_MASK		0x3
#define DRV8703Q1_IDRIVE_WD_CONTROL_IDRIVE		0
#define DRV8703Q1_IDRIVE_WD_CONTROL_IDRIVE_MASK		0x7
#define DRV8703Q1_VDS_CONTROL_REG			4
#define DRV8703Q1_VDS_CONTROL_SO_LIM			7
#define DRV8703Q1_VDS_CONTROL_VDS			4
#define DRV8703Q1_VDS_CONTROL_VDS_MASK			0x7
#define DRV8703Q1_VDS_CONTROL_DIS_H2_LDS		3
#define DRV8703Q1_VDS_CONTROL_DIS_L2_VDS		2
#define DRV8703Q1_VDS_CONTROL_DIS_H1_VDS		1
#define DRV8703Q1_VDS_CONTROL_DIS_L1_VDS		0
#define DRV8703Q1_CONFIG_CONTROL_REG			5
#define DRV8703Q1_CONFIG_CONTROL_TOFF			6
#define DRV8703Q1_CONFIG_CONTROL_TOFF_MASK		0x3
#define DRV8703Q1_CONFIG_CONTROL_CHOP_IDS		5
#define DRV8703Q1_CONFIG_CONTROL_VREF_SCL		3
#define DRV8703Q1_CONFIG_CONTROL_VREF_SCL_MASK		0x3
#define DRV8703Q1_CONFIG_CONTROL_SH_EN			2
#define DRV8703Q1_CONFIG_CONTROL_GAIN_CS		0
#define DRV8703Q1_CONFIG_CONTROL_GAIN_CS_MASK		0x3

typedef struct {
	unsigned int cmd;
	unsigned long arg;
} drv8703q1_arg_t;

/* Used for the calls from the motor control backend */
static void drv8703q1_work_fct(struct work_struct *work);
static struct spi_device *__spi;
static struct work_struct drv8703q1_work;
static struct workqueue_struct *drv8703q1_workqueue;
static drv8703q1_arg_t drv8703q1_arg;

/**
 * Read a register.
 */
static unsigned char drv8703q1_read(unsigned char address, unsigned char data) {
	unsigned char command[2];
	unsigned char response[2];
	struct spi_message m;
	struct spi_transfer t = {
			.tx_buf = command,
			.rx_buf = response,
			.len = 2,
	};

	command[0] = BIT(DRV8703Q1_RW) | ((address & DRV8703Q1_ADDRESS_MASK) << DRV8703Q1_ADDRESS);
	command[1] = data & DRV8703Q1_DATA_MASK;

	DBG("read command: 0x%02x%02x\n", command[0], command[1]);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	spi_sync(__spi, &m);

	DBG("read response: 0x%02x%02x\n", response[0], response[1]);

	return response[1];
}

/**
 * Write into a register.
 */
static void drv8703q1_write(unsigned char address, unsigned char data) {
	unsigned char command[2];

	command[0] = (address & DRV8703Q1_ADDRESS_MASK) << DRV8703Q1_ADDRESS;
	command[1] = data & DRV8703Q1_DATA_MASK;

	DBG("write command: 0x%02x%02x\n", command[0], command[1]);

	spi_write(__spi, command, 2);
}

/**
 * Read the FAULT status register.
 * Bit list:
 * 7 FAULT Logic OR of the FAULT status register excluding the OTW bit
 * 6 WDFLT Watchdog time-out fault
 * 5 GDF R Indicates gate drive fault condition
 * 4 OCP R Indicates VDS monitor overcurrent fault condition
 * 3 VM_UVFL Indicates VM undervoltage lockout fault condition
 * 2 VCP_UVFL Indicates charge-pump undervoltage fault condition
 * 1 OTSD Indicates overtemperature shutdown
 * 0 OTW Indicates overtemperature warning
 */
static unsigned char drv8703q1_read_fault_status(void) {
	return drv8703q1_read(DRV8703Q1_FAULT_STATUS_REG, 0);
}

/**
 * Read the VDS and GDF status register.
 * Bit list:
 * 7 H2_GDF Indicates gate drive fault on the high-side FET of half bridge 2
 * 6 L2_GDF Indicates gate drive fault on the low-side FET of half bridge 2
 * 5 H1_GDF Indicates gate drive fault on the high-side FET of half bridge 1
 * 4 L1_GDF Indicates gate drive fault on the low-side FET of half bridge 1
 * 3 H2_VDS Indicates VDS monitor overcurrent fault on the high-side FET of half bridge 2
 * 2 L2_VDS Indicates VDS monitor overcurrent fault on the low-side FET of half bridge 2
 * 1 H1_VDS Indicates VDS monitor overcurrent fault on the high-side FET of half bridge 1
 * 0 L1_VDS Indicates VDS monitor overcurrent fault on the low-side FET of half bridge 1
 */
static unsigned char drv8703q1_read_vds_gdf_status(void) {
	return drv8703q1_read(DRV8703Q1_VDS_GDF_STATUS_REG, 0);
}

/**
 * Read the main control register.
 * Bit list:
 * 7-6 RESERVED Reserved
 * 5-3 LOCK Write 110b to lock the settings by ignoring further register
 *          changes except to address 0x02h. Writing any sequence other
 *          than 110b has no effect when unlocked.
 *          Write 011b to this register to unlock all registers. Writing any
 *          sequence other than 011b has no effect when locked.
 * 2 IN1/PH This bit is ORed with the IN1/PH pin
 * 1 IN2/EN This bit is ORed with the IN2/EN pin
 * 0 CLR_FLT Write a 1 to this bit to clear the fault bits
 */
static unsigned char drv8703q1_read_main_control(void) {
	return drv8703q1_read(DRV8703Q1_MAIN_CONTROL_REG, 0);
}

/**
 * Read the IDRIVE and WD control register.
 * Bit list:
 * 7-6 TDEAD Dead time
 *           00b = 120 ns
 *           01b = 240 ns
 *           10b = 480 ns
 *           11b = 960 ns
 * 5 WD_EN Time-out of the watchdog timer
 * 4-3 WD_DLY Enables or disables the watchdog timer (disabled by default)
 *            00b = 10 ms
 *            01b = 20 ms
 *            10b = 50 ms
 *            11b = 100 ms
 * 2-0 IDRIVE Sets the peak source current and peak sink current of the gate
 *            drive. Table 22 lists the bit settings.
 */
static unsigned char drv8703q1_read_idrive_wd_control(void) {
	return drv8703q1_read(DRV8703Q1_IDRIVE_WD_CONTROL_REG, 0);
}

/**
 * Read the VDS control register.
 * Bit list:
 * 7 SO_LIM 0b = Default operation
 *          1b = SO output is voltage-limited to 3.6 V
 * 6-4 VDS Sets the VDS(OCP) monitor for each FET
 *         000b = 0.06 V
 *         001b = 0.145 V
 *         010b = 0.17 V
 *         011b = 0.2 V
 *         100b = 0.12 V
 *         101b = 0.24 V
 *         110b = 0.48 V
 *         111b = 0.96 V
 * 3 DIS_H2_VDS Disables the VDS monitor on the high-side FET of half bridge 2
 *              (enabled by default)
 * 2 DIS_L2_VDS Disables the VDS monitor on the low-side FET of half bridge 2
 *              (enabled by default)
 * 1 DIS_H1_VDS Disables the VDS monitor on the high-side FET of half bridge 1
 *              (enabled by default)
 * 0 DIS_L1_VDS Disables the VDS monitor on the low-side FET of half bridge 1
 *              (enabled by default)
 */
static unsigned char drv8703q1_read_vds_control(void) {
	return drv8703q1_read(DRV8703Q1_VDS_CONTROL_REG, 0);
}

/**
 * Read the config control register.
 * Bit list:
 * 7-6 TOFF Off time for PWM current chopping
 *          00b = 25 μs
 *          01b = 50 μs
 *          10b = 100 μs
 *          11b = 200 μs
 * 5 CHOP_IDS Disables current regulation (enabled by default)
 * 4-3 VREF_SCL Scale factor for the VREF input
 *              00b = 100%
 *              01b = 75%
 *              10b = 50%
 *              11b = 25%
 * 2 SH_EN Enables sample and hold operation of the shunt amplifier
 *         (disabled by default)
 * 1-0 GAIN_CS Shunt amplifier gain setting
 *             00b = 10 V/V
 *             01b = 19.8 V/V
 *             10b = 39.4 V/V
 *             11b = 78 V/V
 */
static unsigned char drv8703q1_read_config_control(void) {
	return drv8703q1_read(DRV8703Q1_CONFIG_CONTROL_REG, 0);
}

/**
 * sysfs entry for the FAULT status register.
 */
static ssize_t fault_status_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_fault_status());
}

/**
 * sysfs entry for the VDS and GDF status register.
 */
static ssize_t vds_gdf_status_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_vds_gdf_status());
}

/**
 * sysfs entry for the main control register.
 */
static ssize_t main_control_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_main_control());
}

/**
 * sysfs entry for the IDRIVE and WD control register.
 */
static ssize_t idrive_wd_control_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_idrive_wd_control());
}


/**
 * sysfs entry for the VDS control register.
 */
static ssize_t vds_control_show(struct device *dev, struct device_attribute *attr,
				char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_vds_control());
}

/**
 * sysfs entry for the config control register.
 */
static ssize_t config_control_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 4, "%02x\n", drv8703q1_read_config_control());
}

/**
 * sysfs entry for the config control register.
 */
static ssize_t config_control_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count) {
	drv8703q1_write(DRV8703Q1_CONFIG_CONTROL_REG, 0x07);


	return count;
}

/**
 * sysfs entry for the IDRIVE peak current control.
 */
static ssize_t idrive_peak_current_show(struct device *dev, struct device_attribute *attr,
					char *buf) {
	return snprintf(buf, 3, "%d\n", drv8703q1_read_idrive_wd_control() & 0x7);
}


/**
 * sysfs entry for the IDRIVE peak current control.
 */
static ssize_t idrive_peak_current_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count) {

	uint8_t value = buf[0] - '0';
	unsigned char data;

	data = drv8703q1_read(DRV8703Q1_IDRIVE_WD_CONTROL_REG, 0);
	data &= ~0x7;
	data |= value & 0x7;

	drv8703q1_write(DRV8703Q1_IDRIVE_WD_CONTROL_REG, data);

	return count;
}

/* sysfs entries in read mode */
static DEVICE_ATTR_RO(fault_status);
static DEVICE_ATTR_RO(vds_gdf_status);
static DEVICE_ATTR_RO(main_control);
static DEVICE_ATTR_RO(idrive_wd_control);
static DEVICE_ATTR_RO(vds_control);
static DEVICE_ATTR_RW(config_control);
static DEVICE_ATTR_RW(idrive_peak_current);

/**
 * IN1/PH control.
 */
void drv8703q1_write_in1_ph(bool set) {
	unsigned char data = drv8703q1_read(DRV8703Q1_MAIN_CONTROL_REG, 0);

	if (set)
		data |= BIT(DRV8703Q1_MAIN_CONTROL_IN1_PH);
	else
		data &= ~BIT(DRV8703Q1_MAIN_CONTROL_IN1_PH);

	drv8703q1_write(DRV8703Q1_MAIN_CONTROL_REG, data);
}

EXPORT_SYMBOL(drv8703q1_write_in1_ph);

/**
 * Read the IN1_PH bit.
 * @return 	The stats of the IN1_PH bit
 */
bool drv8703q1_read_in1_ph(void) {
	unsigned char data = drv8703q1_read(DRV8703Q1_MAIN_CONTROL_REG, 0);


	if (data & BIT(DRV8703Q1_MAIN_CONTROL_IN1_PH))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(drv8703q1_read_in1_ph);

/**
 * IN2/EN control.
 */
void drv8703q1_write_in2_en(bool set) {
	unsigned char data = drv8703q1_read(DRV8703Q1_MAIN_CONTROL_REG, 0);

	if (set)
		data |= BIT(DRV8703Q1_MAIN_CONTROL_IN2_EN);
	else
		data &= ~BIT(DRV8703Q1_MAIN_CONTROL_IN2_EN);

	drv8703q1_write(DRV8703Q1_MAIN_CONTROL_REG, data);
}
EXPORT_SYMBOL(drv8703q1_write_in2_en);

/**
 * Read the IN2_EN bit.
 * @return 	The state of the IN2_EN bit
 */
bool drv8703q1_read_in2_en(void) {
	unsigned char data = drv8703q1_read(DRV8703Q1_MAIN_CONTROL_REG, 0);

	if (data & BIT(DRV8703Q1_MAIN_CONTROL_IN2_EN)) {
		lprintk("ENABLED\n");
		return true;
	} else {
		lprintk("DISABLED\n");
		return false;
	}
}
EXPORT_SYMBOL(drv8703q1_read_in2_en);

/**
 * sysfs entry for the IN1/PH control.
 */
static ssize_t in1_ph_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {
	switch (buf[0]) {
	case '0':
		drv8703q1_write_in1_ph(false);
		break;

	case '1':
		drv8703q1_write_in1_ph(true);
		break;

	default:
		lprintk("Bad parameter\n");
		break;
	}

	return count;
}

/**
 * sysfs entry for the IN2/EN control.
 */
static ssize_t in2_en_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {
	switch (buf[0]) {
	case '0':
		drv8703q1_write_in2_en(false);
		break;

	case '1':
		drv8703q1_write_in2_en(true);
		break;

	default:
		lprintk("Bad parameter\n");
		break;
	}

	return count;
}


/**
 * sysfs entry for the IN1/PH control.
 */
static ssize_t wd_en_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {

	unsigned char data = drv8703q1_read(DRV8703Q1_IDRIVE_WD_CONTROL_REG, 0);

	switch (buf[0]) {
	case '0':
		data &= ~BIT(DRV8703Q1_IDRIVE_WD_CONTROL_WD_EN);
		drv8703q1_write(DRV8703Q1_IDRIVE_WD_CONTROL_REG, data);
		break;

	case '1':
		data |= BIT(DRV8703Q1_IDRIVE_WD_CONTROL_WD_EN);
		drv8703q1_write(DRV8703Q1_IDRIVE_WD_CONTROL_REG, data);
		break;

	default:
		lprintk("Bad parameter\n");
		break;
	}

	return count;
}

static ssize_t vds_th_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count) {

	uint8_t value = buf[0]-'0';

	unsigned char data = drv8703q1_read(DRV8703Q1_VDS_CONTROL_REG, 0);

	data &= ~(DRV8703Q1_VDS_CONTROL_VDS_MASK << DRV8703Q1_VDS_CONTROL_VDS);
	data |= (value << DRV8703Q1_VDS_CONTROL_VDS);

	drv8703q1_write(DRV8703Q1_VDS_CONTROL_REG, data);

	return count;
}

/* sysfs entries in write mode */
static DEVICE_ATTR_WO(in1_ph);
static DEVICE_ATTR_WO(in2_en);
static DEVICE_ATTR_WO(wd_en);
static DEVICE_ATTR_WO(vds_th);

static struct attribute *drv8703q1_attributes[] = {
	&dev_attr_fault_status.attr,
	&dev_attr_vds_gdf_status.attr,
	&dev_attr_main_control.attr,
	&dev_attr_idrive_wd_control.attr,
	&dev_attr_vds_control.attr,
	&dev_attr_config_control.attr,
	&dev_attr_idrive_peak_current.attr,
	&dev_attr_in1_ph.attr,
	&dev_attr_in2_en.attr,
	&dev_attr_wd_en.attr,
	&dev_attr_vds_th.attr,
	NULL
};

static const struct attribute_group drv8703q1_attr_group = {
	.attrs = drv8703q1_attributes,
};

static int drv8703q1_probe(struct spi_device *spi) {
	int ret;
	int sleep_gpio;
	int mode_gpio;  // DRV_MODE PIN PD24
	struct device_node *np = spi->dev.of_node;

	printk(KERN_INFO "DRV8703-Q1 Probe\n");

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0)
		return ret;

	// SLEEP\ GPIO request and set
	sleep_gpio = of_get_named_gpio(np, "sleep-gpio", 0);
	if (sleep_gpio == -EPROBE_DEFER)
		return sleep_gpio;
	if (sleep_gpio < 0) {
		dev_err(&spi->dev, "error acquiring reset gpio: %d\n", sleep_gpio);
		return sleep_gpio;
	}
	ret = devm_gpio_request_one(&spi->dev, sleep_gpio, 0, "DRV8703-Q1 sleep");
	if (ret) {
		dev_err(&spi->dev, "error requesting reset gpio: %d\n", ret);
		return ret;
	}
	/* Set the SLEEP\ GPIO pin to 1 */
	gpio_set_value(sleep_gpio, 1);

	/* MODE GPIO request and set */
	mode_gpio = of_get_named_gpio(np, "mode-gpio", 0);
	if (mode_gpio == -EPROBE_DEFER)
		return mode_gpio;
	if (mode_gpio < 0) {
		dev_err(&spi->dev, "error acquiring mode gpio: %d\n", mode_gpio);
		return mode_gpio;
	}
	ret = devm_gpio_request_one(&spi->dev, mode_gpio, 0, "DRV8703-Q1 MODE");
	if (ret) {
		dev_err(&spi->dev, "error requesting mode gpio: %d\n", ret);
		return ret;
	}
	/* Set the MODE GPIO pin to 0. It makes the DRV8703 work in normal mode */
	gpio_set_value(mode_gpio, 0);

	/* Initialize the spi pointer and workqueue structs for calls coming from the vMotor backend */
	__spi = spi;
	drv8703q1_workqueue = create_singlethread_workqueue("drv8703q1");
	if (!drv8703q1_workqueue) {
		lprintk("Cannot create the workqueue\n");
		BUG();
	}
	INIT_WORK(&drv8703q1_work, drv8703q1_work_fct);

	/* Initiate a dummy SPI request to put it in a coherent state */
	drv8703q1_read_fault_status();

	return sysfs_create_group(&spi->dev.kobj, &drv8703q1_attr_group);
}

static int drv8703q1_remove(struct spi_device *spi) {
	sysfs_remove_group(&spi->dev.kobj, &drv8703q1_attr_group);

	return 0;
}

long drv8703q1_process(unsigned int cmd, unsigned long arg) {
	drv8703q1_arg.cmd = cmd;
	drv8703q1_arg.arg = arg;

	schedule_work(&drv8703q1_work);

	return 0;
}

static void drv8703q1_work_fct(struct work_struct *work) {
	switch (drv8703q1_arg.cmd) {
	default:
		break;
	}
}

static const struct spi_device_id drv8703q1_id_table[] = {
	{ "drv8703q1", 0 },
	{  }
};
MODULE_DEVICE_TABLE(spi, drv8703q1_id_table);

#ifdef CONFIG_OF
static const struct of_device_id drv8703q1_of_match[] = {
	{ .compatible = "ti,drv8703q1", },
	{  }
};
MODULE_DEVICE_TABLE(of, drv8703q1_of_match);
#endif

static struct spi_driver drv8703q1_driver = {
	.driver = {
		.name		= "drv8703q1",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(drv8703q1_of_match),
	},
	.probe		= drv8703q1_probe,
	.remove		= drv8703q1_remove,
	.id_table	= drv8703q1_id_table,
};

module_spi_driver(drv8703q1_driver);
