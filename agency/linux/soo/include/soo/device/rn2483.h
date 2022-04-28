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

/* ============================== DTS =================================== */
/* 
To activate add a node to uart definition to bcm2711-<platform>.dts:
	
	&uart<nr> {
		...

        lora-uart {
            compatible = "lora,rn2483";
            current-speed = <57600>
        };
	};
 
 */

#ifndef _LINUX_RN2483_SERDEV_H
#define _LINUX_RN2483_SERDEV_H

#include <linux/serdev.h>

#define RN2483_SERDEV_NAME                      "rn2483_serdev"
#define RN2483_SERDEV_PREFIX                    "[" RN2483_SERDEV_NAME "] "

/*** Serial port default config ***/
#define RN2483_SERDEV_DEFAULT_BAUDRATE          57600
#define RN2483_SERDEV_DEFAULT_PARITY            0   //parity none
#define RN2483_SERDEV_DEFAULT_FLOW_CTRL         false
#define RN2483_SERDEV_DEFAULT_RTS               false
#define RN2483_SEND_TIMEOUT                     10 // in ms


#define RN2483_COMPATIBLE                       "lora,rn2483"

#define MAX_SUBSCRIBERS                         10

/** RN2483 Commands strings **/
#define RN2483_OK                               "ok"
#define RN2483_INVALID_PARAM                    "invalid_param"
#define RN2483_RADIO_ERR                        "radio_err"
 

typedef unsigned char byte;

enum rn2483_status {
    IDLE=0,
    SEND_CMD,
    SEND_MSG,
    LISTEN
};
typedef enum rn2483_status rn2483_status_t;

enum rn2483_cmd {
    none = 0,
    reset,
    get_version,
    mac_pause,
    radio_tx,
    radio_rx,
    stop_rx
};
typedef enum rn2483_cmd rn2483_cmd_t;

static const char cmd_list [][20] = {
    [reset] = "sys reset",
    [get_version] = "sys get ver",
    [mac_pause] = "mac pause",
    [radio_tx] = "radio tx",
    [radio_rx] = "radio rx",
    [stop_rx] = "rxstop"
};

struct rn2483_uart {
    struct serdev_device *serdev;
    struct device *dev;
    unsigned int baud;
    int is_open;

    /** Current mode of the device **/
    rn2483_status_t status;
    rn2483_cmd_t current_cmd;
};

/**
 * @brief Subscribe to tcm515. Every time a new ESP3 packet is received it's sent to all
 *          all subscribers
 * 
 * @param callback Function to be called. Defined by the subscriber
 * @return int 0 on success, -1 on error
 */
int rn2483_subscribe(void *data);

#endif