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
            compatible = "lora,mipot32001353";
            current-speed = <115200>
        };
	};
 
 */

#ifndef _LINUX_MIPOT320_SERDEV_H
#define _LINUX_MIPOT320_SERDEV_H

#include <linux/serdev.h>
#include <linux/completion.h>

#define MIPOT320_SERDEV_NAME                      "mipot32001353_serdev"
#define MIPOT320_SERDEV_PREFIX                    "[" MIPOT320_SERDEV_NAME "] "

/*** Serial port default config ***/
#define MIPOT320_SERDEV_DEFAULT_BAUDRATE        115200
#define MIPOT320_SERDEV_DEFAULT_PARITY          0   /* parity none */
#define MIPOT320_SERDEV_DEFAULT_FLOW_CTRL       false
#define MIPOT320_SERDEV_DEFAULT_RTS             false

#define MIPOT320_COMPATIBLE                     "lora,mipot32001353"
#define MAX_SUBSCRIBERS                          10
#define RESPONSE_TIMEOUT                         1000

/** Bytes **/
#define MIPOT320_HEADER                         0xAA
#define MIPOT320_HEADER_OFFS                    0x0
#define MIPOT320_CMD_OFFS                       0x1
#define MIPOT320_LEN_OFFS                       0x2
#define MIPOT320_DATA_OFFS                      0x3
#define MIPOT320_MIN_MSG_LEN                    0x4               

typedef unsigned char byte;

/** Supported commands **/
enum mipot320_cmd {
    NONE=0x0,
    RESET=0x30,
    FACTORY_RESET=0x31,
    EEPROM_WRITE=0x32,
    EEPROM_READ=0x33,
    GET_FW_VERSION=0x34,
    GET_SR_NUMBER=0x35,
    GET_DEV_EUI=0x36,
    JOIN=0x40,
    JOIN_IND=0x41,
    GET_ACTIVATION_STATUS=0x42,
    SET_APP_KEY=0x43,
    SET_APP_SESSION_KEY=0x44,
    SET_NWK_SESSION_KEY=0x45,
    TX_MSG=0x46,
    TX_MSG_CONFIRMED_IND=0x47,
    TX_MSG_UNCONFIRMED_IND=0x48,
    RX_MSG_IND=0x49,
    GET_SESSION_STATUS=0x4A,
    SET_NEXT_DR=0x4B
};

typedef enum mipot320_cmd mipot320_cmd_t;

struct msg{
    mipot320_cmd_t cmd;
    int length;
    byte *payload;
};

typedef struct msg mipot_msg_t;

/**
 * @brief MIPOT320 struct. Contains all the necessary attributes 
 *          to access a MIPOT320 device and keep track of the current 
 *          state of the device throught the different processes.
 */
struct mipot320_uart {
    /** Access to serial port **/
    struct serdev_device *serdev;

    /** Device **/
    struct device *dev;

    /** Serial port baudrate **/
    unsigned int baud;

    /** Serial port open status **/
    int is_open;

    /** Current command to be executed **/
    mipot320_cmd_t current_cmd;

    /** Completion used to wait for responses **/
    struct completion wait_rsp;
};

/**
 * @brief Subscribe to mipot320. Every time new LoRa data is received it's sent to all
 *          all subscribers
 * 
 * @param callback Function to be called. Defined by the subscriber
 * @return int 0 on success, -1 on error
 */
int mipot320_subscribe(void (*callback)(byte *data));

/**
 * @brief Send data to be sent using LoRa protocol.
 * 
 * @param data Data to send 
 * @param len Length of the string
 */
void mipot320_send_data(byte *data, int len);

#endif