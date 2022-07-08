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

        enocean-uart {
            compatible = "enocean,tcm515";
            current-speed = <57600>
        };
	};
 
 */

#ifndef _LINUX_TCM515_SERDEV_H
#define _LINUX_TCM515_SERDEV_H

#include <linux/serdev.h>
#include <soo/device/tmc515_esp3.h>

#define TCM515_SERDEV_NAME                      "tcm515_serdev"
#define TCM515_SERDEV_PREFIX                    "[" TCM515_SERDEV_NAME "] "

/*** Serial port default config ***/
#define TCM515_SERDEV_DEFAULT_BAUDRATE          57600
#define TCM515_SERDEV_DEFAULT_PARITY            0   //parity none
#define TCM515_SERDEV_DEFAULT_FLOW_CTRL         false
#define TCM515_SERDEV_DEFAULT_RTS               false
#define TCM515_SEND_TIMEOUT                     10 // in ms


#define TCM515_COMPATIBLE                       "enocean,tcm515"

#define MAX_SUBSCRIBERS                         10

#define APP_VERS_SIZE                           4
#define API_VERS_SIZE                           4
#define CHIP_ID_SIZE                            4
#define CHIP_VERS_SIZE                          4
#define APP_DESC_SIZE                           16

enum read_id_fsm {
    GET_APP_VERS,
    GET_API_VERS,
    GET_CHIP_ID,
    GET_CHIP_VERS,
    GET_APP_DESC
};

struct tcm515_uart {
    struct serdev_device *serdev;
    struct device *dev;
    unsigned int baud;
    int is_open;

    int expect_response;
    void (*response_fn)(esp3_packet_t *packet);
};

/**
 * @brief Subscribe to tcm515. Every time a new ESP3 packet is received it's sent to all
 *          all subscribers
 * 
 * @param callback Function to be called. Defined by the subscriber
 * @return int 0 on success, -1 on error
 */
int tcm515_subscribe(void (*callback)(esp3_packet_t *packet));

/**
 * @brief Write data to serial port
 * 
 * @param buffer data to write
 * @param len length of data
 * @param expect_resp true if data will trigger a response
 * @param response_fn callback to treat the response
 * @return int byte written
 */
int tcm515_write_buf(const byte *buffer, size_t len, bool expect_resp, void (*response_fn)(esp3_packet_t *packet));

#endif
