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

        lora {
            compatible = "lora,rn2483";
            current-speed = <57600>
        };
	};
 
 */

#ifndef _LINUX_RN2483_SERDEV_H
#define _LINUX_RN2483_SERDEV_H

#include <linux/serdev.h>
#include <linux/completion.h>

#define RN2483_SERDEV_NAME                      "rn2483_serdev"
#define RN2483_SERDEV_PREFIX                    "[" RN2483_SERDEV_NAME "] "

/*** Serial port default config ***/
#define RN2483_SERDEV_DEFAULT_BAUDRATE          57600
#define RN2483_SERDEV_DEFAULT_PARITY            0   /* parity none */
#define RN2483_SERDEV_DEFAULT_FLOW_CTRL         false
#define RN2483_SERDEV_DEFAULT_RTS               false
#define RN2483_SEND_TIMEOUT                     10 /* in ms */


#define RN2483_COMPATIBLE                       "lora,rn2483"

#define MAX_SUBSCRIBERS                         10

/** RN2483 Commands response strings **/
#define RN2483_OK                               "ok"
#define RN2483_INVALID_PARAM                    "invalid_param"
#define RN2483_RADIO_ERR                        "radio_err"
#define RN2483_RADIO_RX                         "radio_rx"
#define RN2483_RADIO_TX_OK                      "radio_tx_ok"
 
#define RESPONSE_TIMEOUT                        1000
#define DELIM_CHAR                              0x20
#define MAX_CMD_SIZE                            20

typedef unsigned char byte;

enum rn2483_status {
    IDLE=0,
    SEND_CMD,
    SEND_MSG,
    LISTEN
};
typedef enum rn2483_status rn2483_status_t;

/** Supported commands **/
enum rn2483_cmd {
    NONE = 0,
    RESET,
    GET_VERSION,
    MAC_PAUSE,
    RADIO_TX,
    RADIO_RX,
    STOP_RX,
    SET_WDT
};
typedef enum rn2483_cmd rn2483_cmd_t;

/** Commands string **/
static const char cmd_list [][MAX_CMD_SIZE] = {
    [RESET] = "sys reset",
    [GET_VERSION] = "sys get ver",
    [MAC_PAUSE] = "mac pause",
    [RADIO_TX] = "radio tx",
    [RADIO_RX] = "radio rx",
    [STOP_RX] = "radio rxstop",
    [SET_WDT] = "radio set wdt"
};

/**
 * @brief RN2483 struct. Contains all the necessary attributes 
 *          to access a rn2483 device and keep track of the current 
 *          state of the device throught the different processes.
 */
struct rn2483_uart {
    /** Access to serial port **/
    struct serdev_device *serdev;

    /** Device **/
    struct device *dev;

    /** Serial port baudrate **/
    unsigned int baud;

    /** Serial port open status **/
    int is_open;

    /** Current mode of the device **/
    rn2483_status_t status;

    /** Current command to be executed **/
    rn2483_cmd_t current_cmd;

    /** Completion used to wait for responses **/
    struct completion wait_rsp;
};

/**
 * @brief Subscribe to rn2483. Every time new LoRa data is received it's sent to all
 *          all subscribers
 * 
 * @param callback Function to be called. Defined by the subscriber
 * @return int 0 on success, -1 on error
 */
int rn2483_subscribe(void (*callback)(byte *data));

/**
 * @brief Send data to be sent using LoRa protocol.
 * 
 * @param data String of data to send 
 * @param len Length of the string
 */
void rn2483_send_data(char *data, int len);

#endif