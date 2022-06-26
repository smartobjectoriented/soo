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
#include <linux/mutex.h>

#define KBERRY838_SERDEV_NAME                      "kberry838_serdev"
#define KBERRY838_SERDEV_PREFIX                    "[" KBERRY838_SERDEV_NAME "] "

/*** Serial port default config ***/
#define KBERRY838_SERDEV_DEFAULT_BAUDRATE           19200
#define KBERRY838_SERDEV_DEFAULT_PARITY             0   /* parity none */
#define KBERRY838_SERDEV_DEFAULT_FLOW_CTRL          false
#define KBERRY838_SERDEV_DEFAULT_RTS                false
#define KBERRY838_RSP_TIMEOUT                       5000 /* in ms */

#define KBERRY838_COMPATIBLE                        "knx,kberry838"

#define MAX_SUBSCRIBERS                             10


/** FT12 Frame Header Values **/
#define FT12_START_CHAR                             0x68
#define FT12_STOP_CHAR                              0x16
#define FT12_EVEN_FRAME                             0x53
#define FT12_ODD_FRAME                              0x73
#define FT12_EVEN_RSP                               0xD3
#define FT12_ODD_RSP                                0xF3
#define FT12_HEADER_SIZE                            0x05
#define FT12_FOOTER_SIZE                            0x02

/** FT12 Frame offsets **/
#define FT12_START_OFF                              0x00
#define FT12_LENGTH_OFF                             0x01
#define FT12_REPEAT_LENGTH_OFF                      0x02
#define FT12_REPEAT_START_OFF                       0x03
#define FT12_CONTROL_BYTE_OFF                       0x04
#define FT12_START_BAOS_OFF                         0x05

#define FT12_ACK                                    0xE5

typedef unsigned char byte;

/**
 * @brief Acknowledge array
 * 
 */
static const byte kberry838_ack [] = {FT12_ACK};

/**
 * @brief Reset array
 * 
 */
static const byte kberry838_reset_req[] = {
    0x10,
    0x40,
    0x40,
    FT12_STOP_CHAR
};

static const byte kberry838_reset_ind[] = {
    0x10,
    0xC0,
    0xC0,
    FT12_STOP_CHAR
};

/**
 * @brief Status of the FT12 frame decoding process
 * 
 */
enum baos_decode_status {
    CHECK_START1 = 0,
    CHECK_LENGTH,
    GET_SECOND_START,
    GET_CONTROL_FIELD,
    GET_BAOS,
    GET_STOP,
    ERROR
};
typedef enum baos_decode_status baos_decode_status_t;

typedef enum {
    WAIT_FT12_START,
    WAIT_FT12_STOP
} ft12_decode_status;


typedef union {
    byte val;
    struct {
        byte res_2:5;
        byte frame_count:1;
        byte res_1:1;
        byte dir:1;
    };
} ft12_cr;

/**
 * @brief Kberry838 struct. Contains all the necessary attributes 
 *          to interact with the kberry838 device and keep track of the current 
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
    
    /** Parity of the current processed frame **/
    byte request_cr;

    byte response_cr;

    /** Completion used to wait for responses **/
    struct completion wait_rsp;

    ft12_decode_status decode_status;
};

/** Forward declaration of BAOS frame (found in baos_client.h) **/
typedef struct baos_frame baos_frame_t;

/**
 * @brief Send a BAOS frame to the kberry838 device.
 * 
 * @param baos_frame Frame to send
 * @param len Number of byte of the frame
 */
void kberry838_send_data(byte *baos_frame, int len);

#endif