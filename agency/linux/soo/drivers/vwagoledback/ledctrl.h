/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

#ifndef LEDCTRL_H
#define LEDCTRL_H

#define LEDCTRL_NAME		"ledctrl"
#define LEDCTRL_PREFIX		"[" LEDCTRL_NAME "] "

#define IDS_STR_MAX         256
#define ID_STR_SIZE         16
#define NOTIFY_STR_SIZE     20

/* Wago commands */
typedef enum {
    /* Turn on the LEDs */
    LED_ON,
    /* Turn off the LEDs */
    LED_OFF,
    /* Get the current of the LEDs, on/off and later dim value */
    GET_STATUS,
    /* Get a list of the devices connected to the DALI bus */
    GET_TOPOLOGY,
    NONE
} wago_cmd_t;

static const char notify_str [][NOTIFY_STR_SIZE] = {
    [LED_ON] = "led_on",
    [LED_OFF] = "led_off",
    [GET_STATUS] = "get_status",
    [GET_TOPOLOGY] = "get_topology",
    [NONE] = "none"
};

/**
 * @brief Initialize ledctrl. Creates the necessary entries in sysfs
 * 
 * @return int 0 on success, -1 on error
 */
int ledctrl_init(void);

/**
 * @brief Process a request.
 * 
 * @param cmd command to apply to the leds
 * @param ids leds ids to apply the command to
 * @param ids_count number of id in ids array
 */
void ledctrl_process_request(int cmd, int *ids, int ids_count);

#endif //LEDCTRL_H