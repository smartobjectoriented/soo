/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/serial_8250.h>

#include <asm/termios.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>

#include <asm/gpio.h>

#include <stdarg.h>

#include "common.h"

vweather_t vweather;

/* ASCII data coming from the weather station */
static vweather_ascii_data_t ascii_data;

/* Weather data spread over the ecosystem */
static vweather_data_t weather_data;

/*
 * Weather station definitions
 */


/* Entry in /dev matching with UART 4 */
#define WEATHER_UART4_DEV	"ttyS1"

extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);
extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);


/**
 * Rain timer interrupt. This function is called 30 min. after the last pluviometer interrupt.
 * - If another pluviometer interrupt is triggered before the expiry of the timer, this function is not
 *   executed and the state is updated.
 * - If no pluviometer interrupt occurred, this function resets the rain intensity because the rain seems
 *   to be over now.
 */
static void rain_timer_fn(struct timer_list *dummy) {
	DBG("Rain timer interrupt\n");

	weather_data.rain_intensity = NO_RAIN;
}

/**
 * Process incoming frame, update local data and shared data.
 */
void update_weather_data(void) {
	uint32_t i;

	/* South Sun */
	if ((ascii_data.south_sun[0] >= '0') && (ascii_data.south_sun[0] <= '9') &&
		(ascii_data.south_sun[1] >= '0') && (ascii_data.south_sun[1] <= '9')) {
		weather_data.south_sun = ascii_data.south_sun[1] - '0';
		weather_data.south_sun += 10 * (ascii_data.south_sun[0] - '0');
	}

	/* West Sun */
	if ((ascii_data.west_sun[0] >= '0') && (ascii_data.west_sun[0] <= '9') &&
		(ascii_data.west_sun[1] >= '0') && (ascii_data.west_sun[1] <= '9')) {
		weather_data.west_sun = ascii_data.west_sun[1] - '0';
		weather_data.west_sun += 10 * (ascii_data.west_sun[0] - '0');
	}

	/* East Sun */
	if ((ascii_data.east_sun[0] >= '0') && (ascii_data.east_sun[0] <= '9') &&
		(ascii_data.east_sun[1] >= '0') && (ascii_data.east_sun[1] <= '9')) {
		weather_data.east_sun = ascii_data.east_sun[1] - '0';
		weather_data.east_sun += 10 * (ascii_data.east_sun[0] - '0');
	}

	/* Light */
	if ((ascii_data.light[0] >= '0') && (ascii_data.light[0] <= '9') &&
		(ascii_data.light[1] >= '0') && (ascii_data.light[1] <= '9') &&
		(ascii_data.light[2] >= '0') && (ascii_data.light[2] <= '9')) {
		weather_data.light = ascii_data.light[2] - '0';
		weather_data.light += 10 * (ascii_data.light[1] - '0');
		weather_data.light += 100 * (ascii_data.light[0] - '0');
	}

	/* Temperature */
	if ((ascii_data.temperature[1] >= '0') && (ascii_data.temperature[1] <= '9') &&
		(ascii_data.temperature[2] >= '0') && (ascii_data.temperature[2] <= '9') &&
		(ascii_data.temperature[4] >= '0') && (ascii_data.temperature[4] <= '9') &&
		(ascii_data.temperature[3] == '.') &&
		((ascii_data.temperature[0] == '+') || (ascii_data.temperature[0] == '-'))) {
		weather_data.temperature = ascii_data.temperature[4] - '0';
		weather_data.temperature += 10 * (ascii_data.temperature[2] - '0');
		weather_data.temperature += 100 * (ascii_data.temperature[1] - '0');
		if (ascii_data.temperature[0] == '-')
			weather_data.temperature = -weather_data.temperature;
	}

	/* Wind */
	if ((ascii_data.wind[0] >= '0') && (ascii_data.wind[0] <= '9') &&
		(ascii_data.wind[1] >= '0') && (ascii_data.wind[1] <= '9') &&
		(ascii_data.wind[3] >= '0') && (ascii_data.wind[3] <= '9') &&
		(ascii_data.wind[2] == '.')) {
		weather_data.wind = ascii_data.wind[3] - '0';
		weather_data.wind += 10 * (ascii_data.wind[1] - '0');
		weather_data.wind += 100 * (ascii_data.wind[0] - '0');
	}

	/* Twilight */
	if ((ascii_data.twilight[0] == 'J') || (ascii_data.twilight[0] == 'N'))
		weather_data.twilight = (ascii_data.twilight[0] == 'J') ? 1 : 0;

	/* Rain */
	if ((ascii_data.rain[0] == 'J') || (ascii_data.rain[0] == 'N'))
		weather_data.rain = (ascii_data.rain[0] == 'J') ? 1 : 0;

	/* Now update all connected vweather frontends */
	for (i = 0; i < MAX_DOMAINS; i++) {
		if (!vweather_start(i))
			continue;

		memcpy(vweather.weather_buffers[i].data, &weather_data, VWEATHER_DATA_SIZE);
		notify_remote_via_virq(vweather.update_notifications[i].irq);

		vweather_end(i);
	}

}

/**
 * Dump weather ASCII data.
 */
void vweather_dump_ascii_data(void) {
	char output[8];

	VWEATHER_GET_ASCII_DATA(output, ascii_data, frame_begin)
	lprintk("frame_begin: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, temperature)
	lprintk("temperature: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, south_sun)
	lprintk("south_sun: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, west_sun)
	lprintk("west_sun: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, east_sun)
	lprintk("east_sun: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, twilight)
	lprintk("twilight: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, light)
	lprintk("light: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, wind)
	lprintk("wind: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, rain)
	lprintk("rain: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, week_day)
	lprintk("week_day: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, day)
	lprintk("day: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, month)
	lprintk("month: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, year)
	lprintk("year: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, hour)
	lprintk("hour: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, minute)
	lprintk("minute: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, second)
	lprintk("second: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, summer_time)
	lprintk("summer_time: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, checksum)
	lprintk("checksum: %s\n", output);
	VWEATHER_GET_ASCII_DATA(output, ascii_data, frame_end)
	lprintk("frame_end: %d\n", output[0]);
	lprintk("\n");
}

/**
 * Dump weather data.
 */
void vweather_dump_data(void) {
	lprintk("South Sun: %dklx\n", weather_data.south_sun);
	lprintk("West Sun: %dklx\n", weather_data.west_sun);
	lprintk("East Sun: %dklx\n", weather_data.east_sun);
	lprintk("Light: %dlx\n", weather_data.light);
	lprintk("Temperature: %d x 1E-1°C\n", weather_data.temperature);
	lprintk("Wind: %d x 1E-1m/s\n", weather_data.wind);
	lprintk("Twilight: %c\n", (weather_data.twilight) ? 'y' : 'n');
	lprintk("Rain: %c\n", (weather_data.rain) ? 'y' : 'n');
	lprintk("Rain intensity: ");
	switch (weather_data.rain_intensity) {
	case NO_RAIN:
		lprintk("no rain\n");
		break;
	case LIGHT_RAIN:
		lprintk("light\n");
		break;
	case MODERATE_RAIN:
		lprintk("moderate\n");
		break;
	case HEAVY_RAIN:
		lprintk("heavy\n");
		break;
	default:
		lprintk("?\n");
		break;
	}
	lprintk("\n");
}

/**
 * Weather all-in-one station driver
 * The weather station is lined on UART4 of the microcontroller.
 * It appeared as ttyS1 and is reserved for that on SOO.outdoor.
 **/

#ifdef CONFIG_SERIAL_8250

/**
 * Pluviometer interrupt.
 * An interrupt is triggered each 0.2794mm of precipitation.
 * The time interval separating two successive pluviometer interrupts is measured. To evaluate rain amount, the
 * time interval is compared with constant time intervals matching different categories: no rain, low rain, moderate rain,
 * heavy rain.
 */
static irqreturn_t pluvio_interrupt(int irq, void *dev_id) {

	/* Timestamp of the previous pluviometer interrupt */
	static uint64_t prev_rain_timestamp = 0;

	/* Previous state of the rain detector */
	static bool prev_rain_detection = false;

	/* Current timestamp */
	uint64_t cur_rain_timestamp, time_delta;

	/* Current rain detection state */
	bool cur_rain_detection = (weather_data.rain == 1);

	DBG("Pluviometer IRQ\n");

	cur_rain_timestamp = ktime_to_ms(ktime_get());

	/* Wrap handling */
	if (cur_rain_timestamp > prev_rain_timestamp)
		time_delta = cur_rain_timestamp - prev_rain_timestamp;
	else
		time_delta = (0xffffffffffffffffull - prev_rain_timestamp + cur_rain_timestamp);

	if (time_delta < HEAVY_RAIN_INT_DELAY)
		weather_data.rain_intensity = HEAVY_RAIN;
	else if (time_delta < MODERATE_RAIN_INT_DELAY)
		weather_data.rain_intensity = MODERATE_RAIN;
	else if (time_delta < LIGHT_RAIN_INT_DELAY)
		weather_data.rain_intensity = LIGHT_RAIN;
	else
		weather_data.rain_intensity = NO_RAIN;

	prev_rain_timestamp = cur_rain_timestamp;
	prev_rain_detection = cur_rain_detection;

	mod_timer(&weather_data.rain_timer, jiffies + (30 * 60 * HZ)); /* 30 min. */

	return IRQ_HANDLED;
}

/**
 * Setup of the GPIOs required for UART transmission RTS pin and pluviometer pin.
 * This function forces the UART4_RTS pin to 0 to enable the stream coming from the weather station.
 */
static void setup_gpios(void) {
	int ret, gpio_irq;

	/* UART4 RTS GPIO */

	ret = gpio_request(UART4_RTS_GPIO, "vWeather_RTS");
	BUG_ON(ret < 0);

	/* Force UART4_RTS to 1 */
	gpio_direction_output(UART4_RTS_GPIO, 0);
	gpio_set_value(UART4_RTS_GPIO, 1);

	/* The RTS LED (D6) should now be off */

	/* Pluviometer GPIO */

	ret = gpio_request(PLUVIO_GPIO, "vWeather_pluvio");
	BUG_ON(ret < 0);

	gpio_direction_input(PLUVIO_GPIO);

	/* Request an IRQ bound to the pluviometer GPIO */
	gpio_irq = gpio_to_irq(PLUVIO_GPIO);
	BUG_ON(gpio_irq < 0);

	ret = request_irq(gpio_irq, pluvio_interrupt, IRQF_TRIGGER_FALLING, "pluvio_irq", NULL);
	BUG_ON(ret < 0);
}

/**
 * Receival of the frames coming from the weather station by reading the serial line.
 */
static int weather_monitor_fn(void *args) {
	struct tty_struct *tty_uart;
	int len, nbytes;
	char buffer[VWEATHER_FRAME_SIZE];
	dev_t dev;
	int baud = 19200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	lprintk("%s: starting to acquire weather data from the weather station.\n", __func__);

	/* Initiate the tty device dedicated to the weather station. */
	tty_dev_name_to_number(WEATHER_UART4_DEV, &dev);
	tty_uart = tty_kopen(dev);

	uart_do_open(tty_uart);

	/* Set the termios parameters related to tty. */

	tty_uart->termios.c_lflag = ECHO | ECHOE | NOFLSH;
	tty_set_termios(tty_uart, &tty_uart->termios);

	/* Set UART configuration */
	uart_set_options(((struct uart_state *) tty_uart->driver_data)->uart_port, NULL, baud, parity, bits, flow);

	while (true) {

		/* According to the doc, we expect 40 bytes starting with 'W' and
		 * finishing with '3'.
		 */
		nbytes = 0;
		while (nbytes < VWEATHER_FRAME_SIZE) {
			len = tty_do_read(tty_uart, buffer + nbytes, VWEATHER_FRAME_SIZE);
			nbytes += len;
		}

		if (!((nbytes == VWEATHER_FRAME_SIZE) && (buffer[0] == 'W') && (buffer[VWEATHER_FRAME_SIZE-1] == 3)))
			continue;

		/* Update the local data with the new received data. */
		memcpy(&ascii_data, buffer, VWEATHER_FRAME_SIZE);
		update_weather_data();

	}

	return 0;
}

#endif /* CONFIG_SERIAL_8250 */

void vweather_probe(struct vbus_device *dev) {
	static bool weather_station_initialized = false;

	DBG(VWEATHER_PREFIX "Backend probe: %d\n", dev->otherend_id);

	if (!weather_station_initialized) {

#ifdef CONFIG_SERIAL_8250
		/* Set up the GPIO on UART4 */
		setup_gpios();

		kthread_run(weather_monitor_fn, NULL, "weather_station_monitor");
#endif
		weather_station_initialized = true;

	}
}

void vweather_close(struct vbus_device *dev) {
	DBG(VWEATHER_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vweather_suspend(struct vbus_device *dev) {
	DBG(VWEATHER_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vweather_resume(struct vbus_device *dev) {
	DBG(VWEATHER_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vweather_reconfigured(struct vbus_device *dev) {
	DBG(VWEATHER_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vweather_connected(struct vbus_device *dev) {
	DBG(VWEATHER_PREFIX "Backend connected: %d\n", dev->otherend_id);
}

int vweather_init(void) {
	int ret;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vweather,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vweather_vbus_init();

	memset(&ascii_data, 0, sizeof(ascii_data));
	memset(&weather_data, 0, sizeof(weather_data));

	/* Initialize the rain timer */
	timer_setup(&weather_data.rain_timer, rain_timer_fn, 0);

	/* Set the device capability associated to this interface */
	devaccess_set_devcaps(DEVCAPS_CLASS_DOMOTICS, DEVCAP_WEATHER_DATA, true);

	return ret;
}

module_init(vweather_init);
