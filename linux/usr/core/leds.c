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

#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <core/debug.h>
#include <core/types.h>
#include <core/leds.h>

static FILE *f_led[SOO_N_LEDS];

/**
 * Set the brightness of a LED.
 * The id value is between 1 and SOO_N_LEDS.
 */
void led_set(uint32_t id, uint8_t value) {
	char value_str[4] = { 0 };

	sprintf(value_str, "%u", value);

	rewind(f_led[id - 1]);
	fwrite(value_str, 1, strlen(value_str) + 1, f_led[id - 1]);
	fflush(f_led[id - 1]);
}

/**
 * Set the brightness of a LED to the maximal value.
 * The id value is between 1 and SOO_N_LEDS.
 */
void led_on(uint32_t id) {
	led_set(id, 255);
}

/**
 * Set the brightness of a LED to the minimal value.
 * The id value is between 1 and SOO_N_LEDS.
 */
void led_off(uint32_t id) {
	led_set(id, 0);
}


void leds_init(void) {
	uint8_t i;
	char sysfs_led[64];
	char led_id[2] = { 0 };

	for (i = 0 ; i < SOO_N_LEDS ; i++) {
		memset(sysfs_led, 0, 64);
		strcpy(sysfs_led, SOO_LED_SYSFS_PREFIX);
		led_id[0] = i + 1 + '0';
		strcat(sysfs_led, led_id);
		strcat(sysfs_led, SOO_LED_SYSFS_SUFFIX);

		if ((f_led[i] = fopen(sysfs_led, "w")) == NULL) {
			printf("Failed to open LED device (%s)\n", sysfs_led);
			BUG();
		}
	}
}
