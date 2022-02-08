/*
 * Copyright (C) 2020 David Truan <david.truan@@heig-vd.ch>
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
 *
 * This code is a porting of the 'EasyCAT.h' code from AB&T Tecnologie
 * Informatiche - Ivrea Italy (http://www.bausano.net)

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

#include <linux/gpio/consumer.h>
#include <linux/pwm.h>
#include <linux/i2c.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

#include <rtdm/rtdm.h>
#include <xenomai/rtdm/driver.h>

#include <linux/ipipe.h>

#include "companion.h"

static struct companion_status companion;

/* PWM names to be retrieved from the DTS */
const char *pwm_names[NB_PWM] = { "pwm00", "pwm01" };

/**
 * Read a direct register.
 */
static uint32_t companion_read_reg_direct(uint16_t address) {
	// unsigned char command[4] = {0};
	unsigned char command[RD_BUF_TX_LEN] = {0};
	unsigned char response[RD_BUF_RX_LEN] = {0};
	struct spi_transfer xfers[2];
	struct spi_transfer tt = {
			.tx_buf = command,
			.len = RD_BUF_TX_LEN,
			.speed_hz = 1000000,
	};
	struct spi_transfer tr = {
			.rx_buf = response,
			.len = RD_BUF_RX_LEN,
			.speed_hz = 1000000,
	};
	xfers[0] = tt;
	xfers[1] = tr;

	command[0] = COMM_SPI_READ;
	command[1] = (address >> 8) & 0xFF;
	command[2] = address & 0xFF;

	DBG("read addr: 0x%04X\n", address);

	spi_sync_transfer(companion.spi, xfers, 2);

	DBG("read response: 0x%02X%02X%02X%02X\n", response[3], response[2], response[1], response[0]);

	return *((uint32_t *)response);
}

/**
 * Write into a direct register.
 */
static void companion_write_reg_direct(uint16_t address, uint32_t data) {
	unsigned char command[WR_BUF_TX_LEN] = {0};

	command[0] = COMM_SPI_WRITE;
	command[1] = (address >> 8) & 0xFF;
	command[2] = address & 0xFF;

	command[6] = (data >> 24) & 0xFF;
	command[5] = (data >> 16) & 0xFF;
	command[4] = (data >> 8) & 0xFF;
	command[3] = data & 0xFF;

	DBG("write at 0x%04X data: 0x%08X\n", address, data);

	spi_write(companion.spi, command, WR_BUF_TX_LEN);
}

/**
 * Read indirect using the ECAT_CSR_CMD register.
 */
static uint32_t companion_read_reg_indirect(uint16_t address, uint32_t len) {
	uint32_t data = 0;

	data = address | ((uint8_t)len << 16) | (ESC_READ << 24);

	companion_write_reg_direct(ECAT_CSR_CMD, data);

	do {
		data = companion_read_reg_direct(ECAT_CSR_CMD);
	} while(data & ECAT_CSR_BUSY);

	return companion_read_reg_direct(ECAT_CSR_DATA);
}

/**
 * Write indirect using the ECAT_CSR_CMD register.
 */
static void companion_write_reg_indirect(uint16_t address, uint32_t dataOut, uint32_t len) {
	uint32_t data = 0;

	companion_write_reg_direct(ECAT_CSR_DATA, dataOut);

	data = address | ((uint8_t)len << 16) | (ESC_WRITE << 24);
	companion_write_reg_direct(ECAT_CSR_CMD, data);

	do {
		data = companion_read_reg_direct(ECAT_CSR_CMD);
	} while(data & ECAT_CSR_BUSY);
}

/**
 * Read from process RAM FIFO into 'out' PDOs buffers
 */
static void companion_read_ram(void)
{
	// unsigned char command[4] = {0};
	unsigned char command[RD_BUF_TX_LEN] = {0};
	struct spi_transfer xfers[2];
	struct spi_transfer tt = {
			.tx_buf = command,
			.len = RD_BUF_TX_LEN,
			.speed_hz = 1000000,
	};
	struct spi_transfer tr = {
			.rx_buf = companion.buffers.out,
			.len = 32,
			.speed_hz = 1000000,
	};
	xfers[0] = tt;
	xfers[1] = tr;

	command[0] = COMM_SPI_READ;
	command[1] = 0x00;
	command[2] = 0x00;

	spi_sync_transfer(companion.spi, xfers, 2);
}

/**
 * Write into process RAM FIFO the 'in' PDOs buffers
 */
static void companion_write_ram(void)
{
	// unsigned char command[4] = {0};
	unsigned char command[RD_BUF_TX_LEN] = {0};
	struct spi_transfer xfers[2];
	struct spi_transfer tt = {
			.tx_buf = command,
			.len = RD_BUF_TX_LEN,
			.speed_hz = 1000000,
	};
	struct spi_transfer tr = {
			.tx_buf = companion.buffers.in,
			.len = 32,
			.speed_hz = 1000000,
	};
	xfers[0] = tt;
	xfers[1] = tr;

	command[0] = COMM_SPI_WRITE;
	command[1] = 0x00;
	command[2] = 0x20;

	spi_sync_transfer(companion.spi, xfers, 2);
}

/**
 * Read the PDOs values
 *
 * These are the bytes received from the EtherCAT master and that will be use by
 * our application to write the outputs
 *
 */
static void companion_read_pdos(void)
{
	uint8_t status; /* TODO rename based on LAN9252 spec */
	uint32_t reg_val;

	/* Abort any possible pending transfer */
	companion_write_reg_direct(ECAT_PRAM_RD_CMD, PRAM_ABORT);

	companion_write_reg_direct(ECAT_PRAM_RD_ADDR_LEN, (0x00001000 | (((uint32_t)EC_BYTE_PDO_NUM) << 16)));

	/* Start the command */
	companion_write_reg_direct (ECAT_PRAM_RD_CMD, 0x80000000);

	/* Wait for the data to be  process ram to the read fifo */
#warning Check if this loop is needed for our implementation (32 bytes PDOs only)
	do {
		reg_val = companion_read_reg_direct (ECAT_PRAM_RD_CMD);
		status = (reg_val & 0x00001FF00) >> 8;

	} while (status != (EC_BYTE_PDO_NUM/4));

	companion_read_ram();
}

/**
 * Write data to the input process ram, through the fifo
 *
 * These are the bytes that we have read from the inputs of our application and
 * that will be sent to the EtherCAT master
 *
 */
static void companion_write_pdos(void)
{
	uint32_t reg_val;

	/* Abort any possible pending transfer */
	companion_write_reg_direct (ECAT_PRAM_WR_CMD, PRAM_ABORT);

	companion_write_reg_direct (ECAT_PRAM_WR_ADDR_LEN, (0x00001200 | (((uint32_t)EC_BYTE_PDO_NUM) << 16)));

	/* Start the command */
	companion_write_reg_direct (ECAT_PRAM_WR_CMD, 0x80000000);

	/* check that the fifo has enough free space */
#warning Check if this loop is needed for our implementation (32 bytes PDOs only)
	do {
	  reg_val = companion_read_reg_direct (ECAT_PRAM_WR_CMD);

	} while (reg_val < (EC_BYTE_PDO_NUM/4));

	companion_write_ram();
}

/*
 * EtherCAT Main task - read/write PDOs
 */
static void companion_ec_task(void)
{
	bool watchdog = true;
 	bool operational = false;
	uint32_t reg_val;

  	unsigned char i;

  	/* Check watchdog status */
	reg_val = companion_read_reg_indirect(EC_WDOG_STATUS, 1);
	if (reg_val & 0x0001)
		watchdog = false;
	else
		watchdog = true;

	/* Get EtherCAT State Machine status */
  	reg_val = companion_read_reg_indirect(EC_AL_STATUS, 1);
  	if ((reg_val & 0x000000F) == ESM_OP)
		operational = true;
  	else
		operational = false;

	/* If watchdog is active or we are not in operational state, reset the output
	   buffer otherwise transfer process data from the EtherCAT core to the output
	   buffer  */
	if (watchdog | !operational) {
		for (i=0; i < EC_BYTE_PDO_NUM ; i++)
			companion.buffers.out[i] = 0;

 #ifdef DEBUG
		if (!operational)
			DBG("Not operational\n");
		if (watchdog)
			DBG("WatchDog\n");
 #endif
  	} else {
		companion_read_pdos();
	}

	/* Transfer process data from the input buffer to the EtherCAT core */
	companion_write_pdos();
}

#define NB_GPIOS_IN 	4
#define NB_GPIOS_OUT 	4

static void process_gpios(void)
{
	int i;
	uint8_t in_buf = 0;
	uint8_t out_buf = 0;

	/* Read the inputs and update the EC buffer */
	for (i = 0; i < NB_GPIOS_IN; ++i) {
		if (gpiod_get_value(companion.gpios.gpios_in[i])) {
			in_buf |= 1 << i;
		}
	}
	companion.buffers.in[PDO_INPUTS_GPIO] = in_buf;

	/* Write the outputs accordingly to the EC buffer */
	out_buf = companion.buffers.out[PDO_OUTPUTS_GPIO];
	for (i = 0; i < NB_GPIOS_OUT; ++i) {
		gpiod_set_value(companion.gpios.gpios_out[i], out_buf & (1 << i));
	}
}


/**
 * Setup the IN/OUT GPIOs using gpiod.
 * The error is handled directly in this function so we don't return anything.
 */
static void setup_gpios(struct device *dev)
{
	int i;

	for (i = 0; i < NB_GPIOS_IN; ++i) {
		companion.gpios.gpios_in[i] = gpiod_get_index(dev, "in", i, GPIOD_IN);
		if (IS_ERR(companion.gpios.gpios_in[i])) {
			printk("Error retriving INPUT gpios\n");
			BUG();
		}
	}
	for (i = 0; i < NB_GPIOS_OUT; ++i) {
		companion.gpios.gpios_out[i] = gpiod_get_index(dev, "out", i, GPIOD_OUT_LOW);
		if (IS_ERR(companion.gpios.gpios_out[i])) {
			printk("Error retriving OUTPUT gpios\n");
			BUG();
		}
	}
}

static void companion_pwm_set_frequency(int freq, int pwm_no)
{
	struct pwm_state ps;

	pwm_get_state(companion.pwm[pwm_no], &ps);
	ps.period = 1000000000 / freq;
	pwm_apply_state(companion.pwm[pwm_no], &ps);
}

/* The duty cycle is expressed in 1/200 steps. */
static void companion_pwm_set_duty_cycle(int dc, int pwm_no)
{
	struct pwm_state s;

	pwm_get_state(companion.pwm[pwm_no], &s);
	pwm_set_relative_duty_cycle(&s, dc, 200);
	pwm_apply_state(companion.pwm[pwm_no], &s);
}


static int setup_pwm_no(struct device *dev, int pwm_no)
{
	int ret;
	/* We need to defer the probing if the PWM controller is not probed already */
	companion.pwm[pwm_no] = devm_pwm_get(dev, pwm_names[pwm_no]);
	if (IS_ERR(companion.pwm[pwm_no])) {
		printk("COMPANION: Error retriving the PWM interface! error: %ld!\n", PTR_ERR(companion.pwm));
		return -1;
	}

	ret = pwm_config(companion.pwm[pwm_no], 500000, 1000000);
	if (ret) {
		printk("Companion: Couldn't config the PWM %d\n", pwm_no);
		return -1;
	}

	companion_pwm_set_frequency(2000, pwm_no);
	companion_pwm_set_duty_cycle(100, pwm_no);

	return 0;
}

static int setup_pwms(struct device *dev)
{
	int i;

	for (i = 0; i < NB_PWM; ++i) {
		if (setup_pwm_no(dev, i)) {
			return -1;
		}
	}
	return 0;
}

/* LUT to translate the pwm_no into the corresponding duty-cycle PDO */
static int PWM_DC_PDOS[2] = {PDO_DUTY_CYCLE_0, PDO_DUTY_CYCLE_1};

static void process_pwm(int pwm_no)
{
	struct pwm_state ps;
	int cur_dc;
	int new_dc;

	new_dc = companion.buffers.out[PWM_DC_PDOS[pwm_no]];
	pwm_get_state(companion.pwm[pwm_no], &ps);
	cur_dc = pwm_get_relative_duty_cycle(&ps, 200);

	if (new_dc != cur_dc) {
		printk("Changing PWM[%d] DC:%d -> %d\n", pwm_no, cur_dc, new_dc);
		companion_pwm_set_duty_cycle(new_dc, pwm_no);
	}
}


/**
 * Companion application Thread
 *
 *  Its main tasks are reading/writing PDOS and applies values to the outputs
 */
#if 0 /* kept as reference - has to be removed on application tested */
static int companion_fn(void *data)
{
	// uint8_t gpio;
	// int cpt = 0;
	int i = 0;

	while (1) {

		companion_ec_task();
		process_gpios();

		// get_temperature(companion.i2c_client);

		for (i = 0; i < NB_PWM; ++i) {
			process_pwm(i);
		}

		// for (i = 0; i < 4; ++i) {
		// 	printk("buf.out[0x%X] = %d\n", i, companion.buffers.out[i]);
		// }

		// gpio = companion.buffers.out[0];

		// if (cpt >= 10) {
		// 	printk("gpio: 0x%x\n", gpio);
		// 	cpt = 0;
		// }
		// cpt++;

		// companion.buffers.in[0] = gpio;

		msleep(100);
	}

	printk("WARNING - ETHERCAT THREAD stopped\n");

	return 0;
}
#endif


static irqreturn_t ec_irq_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}


static irqreturn_t ec_threaded_irq_handler(int irq, void *dev_id)
{
	int i = 0;

	companion_ec_task();
	process_gpios();

	for (i = 0; i < NB_PWM; ++i) {
		process_pwm(i);
	}

	return IRQ_HANDLED;
}


#if 0 /* RTDM irq request */
static int companion_rtdm_ec_handler(rtdm_irq_t *irq_handle)
{
	printk("RTDM IRQ %d!!!!!\n", smp_processor_id());
	return RTDM_IRQ_HANDLED;
}

extern void bcm2835_print_reg(struct irq_desc *desc);

static void companion_rt_task(void *arg)
{
	int ret;
	struct spi_device *spi = (struct spi_device *) arg;
	/* Setup the EC IRQ gpio (should be gpio 17) */
	companion.ec_irq_gpio = gpiod_get(&spi->dev, "ec", GPIOD_IN);
	if (IS_ERR(companion.ec_irq_gpio)) {
		printk("Error retriving EC_IRQ gpio %ld\n", PTR_ERR(companion.ec_irq_gpio));
		BUG();
	}
	gpiod_direction_input(companion.ec_irq_gpio);
	companion.ec_irq_no = gpiod_to_irq(companion.ec_irq_gpio);

	ret = rtdm_irq_request(&companion.irq_handle, 17, companion_rtdm_ec_handler, IRQF_TRIGGER_RISING, "companion-ec", NULL);

	if (ret != 0) {
		printk("RTDM IRQ NOT REQUESTED CORRECTLY\n");
		BUG();
	}
	while(1) {
		rtdm_task_sleep(1000000);
	}
}
#endif

static int companion_probe(struct spi_device *spi)
{
	int ret, i;
	uint32_t rval = 1;

	DBG("Companion Probe\n");

	spi->mode = SPI_MODE_0 | SPI_CS_HIGH;
	ret = spi_setup(spi);
	if (ret < 0)
		return ret;

	/* TODO: Priv struct!! */
	companion.spi = spi;

	companion_write_reg_direct(RESET_CTL, DIGITAL_RST);
	/* Reset of the LAN9252.
	   We wait for the reset to be done before continuing the execution
	 */
	do {
		rval = companion_read_reg_direct(RESET_CTL);
	} while ((rval & 0x00000001) != 0);

	/* Wait for the BYTE_TEST register to contain the magic number 0x87654321.
	   If it does, it means the reset is done and we can start using the LAN9252.
	 */
	do {
		rval = companion_read_reg_direct(BYTE_TEST);
	} while (rval != 0x87654321);

	do {
		rval = companion_read_reg_direct(HW_CFG);
	} while (!(rval & READY));

	rval = companion_read_reg_direct(ID_REV);

	printk("****************************** LAN9252 chip %x, rev %x\n", rval >> 16, rval & 0x0000ffff);

	setup_gpios(&spi->dev);

	/* PWMs setup */
	if (setup_pwms(&spi->dev) != 0) {
		goto pwm_err;
	}

	pwm_enable(companion.pwm[0]);
	pwm_enable(companion.pwm[1]);
	companion_pwm_set_frequency(1000, 1);
	companion_pwm_set_duty_cycle(10, 1);

	/* Init our I2C part of the companion driver. ENABLE ONLY IF THE I2C module is connected! */
	// ret = companion_register_i2c_driver(&companion);

#if 0
	/* Initiate our RT task which sets up the RT IRQ and then should wait on a rtdm_event */
	rtdm_task_init(&companion.rt_task, "companion-rt-task", companion_rt_task, spi, 50, 0);
#else

	/* Setup the EC IRQ gpio (should be gpio 17) */
	companion.ec_irq_gpio = gpiod_get(&spi->dev, "ec", GPIOD_IN);
	if (IS_ERR(companion.ec_irq_gpio)) {
		printk("Error retriving EC_IRQ gpio %ld\n", PTR_ERR(companion.ec_irq_gpio));
		BUG();
	}
	companion.ec_irq_no = gpiod_to_irq(companion.ec_irq_gpio);

	ret = devm_request_threaded_irq(&spi->dev, companion.ec_irq_no, ec_irq_handler, ec_threaded_irq_handler, IRQF_TRIGGER_RISING, "ec", NULL);

#endif

	/* DC IRQ (SYNC0) */
	companion_write_reg_indirect(0x0204, 0x00000004, 4);
	/* SM IRQ (Sync Manager 2) */
	// companion_write_reg_indirect(0x0204, 0x00000100, 4);
	companion_write_reg_direct(IRQ_CFG, 0x00000111);
	companion_write_reg_direct(INT_EN, 0x00000001);

	return 0;
pwm_err:
	for (i = 0; i < NB_GPIOS_IN; ++i) {
		gpiod_put(companion.gpios.gpios_in[i]);
	}
	for (i = 0; i < NB_GPIOS_OUT; ++i) {
		gpiod_put(companion.gpios.gpios_out[i]);
	}
	gpiod_put(companion.ec_irq_gpio);

	return -EPROBE_DEFER;
}

static int companion_remove(struct spi_device *spi)
{
	DBG("COMPANION remove!\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id companion_of_match[] = {
	{ .compatible = "bcm,companion" },
	{ },
};
MODULE_DEVICE_TABLE(of, rpisense_core_id);
#endif

static const struct spi_device_id companion_id_table[] = {
	{ "companion", 0 },
	{  }
};
MODULE_DEVICE_TABLE(spi, companion_id_table);


static struct spi_driver companion_driver = {
	.driver = {
		.name		= "companion",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(companion_of_match),
	},
	.probe		= companion_probe,
	.remove		= companion_remove,
	.id_table	= companion_id_table,
};

module_spi_driver(companion_driver);
