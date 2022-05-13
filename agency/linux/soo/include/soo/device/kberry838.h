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

        knx {
            compatible = "knx,kberry838";
            current-speed = <19200>
        };
	};
 
 */

#ifndef _LINUX_KBERRY838_SERDEV_H
#define _LINUX_KBERRY838_SERDEV_H

#include <linux/serdev.h>
#include <linux/completion.h>
#include <linux/types.h>
#include <linux/list.h>

#define KBERRY838_SERDEV_NAME                      "kberry838_serdev"
#define KBERRY838_SERDEV_PREFIX                    "[" KBERRY838_SERDEV_NAME "] "

/*** Serial port default config ***/
#define KBERRY838_SERDEV_DEFAULT_BAUDRATE           19200
#define KBERRY838_SERDEV_DEFAULT_PARITY             0   /* parity none */
#define KBERRY838_SERDEV_DEFAULT_FLOW_CTRL          false
#define KBERRY838_SERDEV_DEFAULT_RTS                false
#define KBERRY838_RSP_TIMEOUT                       1000 /* in ms */

#define KBERRY838_COMPATIBLE                        "knx,kberry838"

#define MAX_SUBSCRIBERS                             10

#define KBERRY838_ACK                               0xE5

/** FT12 Frame Header Values **/
#define FT12_START_CHAR                             0x68
#define FT12_STOP_CHAR                              0x16
#define FT12_EVEN_FRAME                             0x53
#define FT12_ODD_FRAME                              0x73
#define FT12_HEADER_SIZE                            0x05
#define FT12_END_FRAME_SIZE                         0x02

/** FT12 Frame offsets **/
#define FT12_START_OFF                              0x00
#define FT12_LENGTH_OFF                             0x01
#define FT12_REPEAT_LENGTH_OFF                      0x02
#define FT12_REPEAT_START_OFF                       0x03
#define FT12_CONTROL_BYTE_OFF                       0x04
#define FT12_START_BAOS_OFF                         0x05


typedef unsigned char byte;

static const byte kberry838_ack [] = {KBERRY838_ACK};

static const byte kberry838_reset[] = {
    0x10,
    0x40,
    0x40,
    FT12_STOP_CHAR
};

enum baos_decode_status {
    GET_START = 0,
    GET_LENGTH,
    GET_CONTROL_FIELD,
    GET_BAOS,
    GET_STOP
};
typedef enum baos_decode_status baos_decode_status_t;

enum kberry838_status {
    IDLE = 0,
    SEND_RST,
    SEND_MSG,
    WAIT_DATA
};
typedef enum kberry838_status kberry838_status_t;

/**
 * @brief RN2483 struct. Contains all the necessary attributes 
 *          to access a rn2483 device and keep track of the current 
 *          state of the device throught the different processes.
 */
struct kberry838_uart {
    /** Access to serial port **/
    struct serdev_device *serdev;

    /** Device **/
    struct device *dev;

    /** Serial port baudrate **/
    unsigned int baud;

    /** Serial port open status **/
    int is_open;

    int even;

    kberry838_status_t status;

    /** Completion used to wait for responses **/
    struct completion wait_rsp;
};

typedef struct baos_frame baos_frame_t;

void kberry838_send_data(byte *baos_frame, int len);

#endif