/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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


#ifndef RPISENSE_LED_H
#define RPISENSE_LED_H

/* 8 (row) * 8 (column) * 3 (colors) + 1 (first byte needed blank) */
#define SIZE_FB 193

/* LED address on the I2C bus */
#define LED_ADDR	0x46


void senseled_init(void);
void display_led(int led_nr, bool on);

#endif /* RPISENSE_LED_H */
