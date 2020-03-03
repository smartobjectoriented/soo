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

#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>

#include <uapi/soo.h>

#define SOO_N_LEDS				6

#define SOO_LED_SYSFS_PREFIX			"/sys/class/leds/led_d"
#define SOO_LED_SYSFS_SUFFIX			"/brightness"

void led_set(uint32_t id, uint8_t value);
void led_on(uint32_t id);
void led_off(uint32_t id);

void leds_init(void);

#endif /* LEDS_H */
