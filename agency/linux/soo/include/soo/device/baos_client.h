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

#ifndef _LINUX_BAOS_CLIENT_H
#define _LINUX_BAOS_CLIENT_H

#include <linux/types.h>
#include <linux/list.h>

#include <soo/device/kberry838.h>

#define BAOS_CLIENT_NAME		                    "baos_client"
#define BAOS_CLIENT_PREFIX		                    "[" BAOS_CLIENT_NAME " ] "

#define BAOS_MAIN_SERVICE                           0xF0
#define BAOS_START_OBJECT_SIZE                      0x02
#define BAOS_COUNT_OBJECT_SIZE                      0x02
#define BAOS_OBJECT_ID_SIZE                         0x02
#define BAOS_FRAME_MIN_SIZE                         0x06
#define DATAPOINT_MIN_SIZE                          0x04

/** BAOS Frame offsets **/
#define BAOS_MAIN_SERVICE_OFF                       0x00
#define BAOS_SUBSERVICE_OFF                         0x01
#define BAOS_START_OBJECT_OFF                       0x02
#define BAOS_OBJECT_COUNT_OFF                       0x04
#define BAOS_FIRST_OBJECT_OFF                       0x06

/** BAOS items id **/
#define BAOS_ID_SERIAL_NUM                          0x08
#define BAOS_PROTOCOL_VERS                          0x10

#define BAOS_RESPONSE_TIMEOUT                       5000 /** in ms **/

#define BAOS_SUBSERVICE_RESPONSE_OFF                0x80    

#define STRING_SIZE                                 64
#define MAX_SUBSCRIBERS                             10

/**
 * @brief  BAOS subservice codes. See datasheet
 * 
 */
typedef enum {
    GET_SERVER_ITEM = 0x01,
    SET_SERVER_ITEM = 0x02,
    SERVER_ITEM_INDICATION = 0xC2,
    GET_DATAPOINT_DESC = 0x03,
    GET_DESCRIPTION_STRING = 0x04,
    GET_DATAPOINT_VALUE = 0x05,
    DATAPOINT_VALUE_INDICATION = 0xC1,
    SET_DATAPOINT_VALUE = 0x06,
    GET_DATAPOINT_BYTE = 0x07,
    SET_DATAPOINT_HISTORY_COMMAND = 0x08,
    GET_DATAPOINT_HISTORY_STATE = 0x09,
    GET_DATAPOINT_HISTORY = 0x0A
} baos_subservices;

/**
 * @brief BAOS datapoint command codes. See datasheet
 * 
 */
typedef enum {
    NO_COMMAND = 0b0000,
    SET_NEW_VALUE = 0b0001,
    SEND_VALUE_ON_BUS = 0b0010,
    SET_NEW_VALUE_AND_SEND_ON_BUS = 0b0011,
    READ_NEW_VALUE_VIA_BUS = 0b0100,
    CLEAR_DATA_POINT_TRANSMISSION_STATE = 0b0101
} datapoint_commands;

/**
 * @brief BAOS frame type
 * 
 */
typedef enum {
    SERVER_ITEM = 0,
    DATAPOINT,
} baos_frame_type;

/**
 * @brief BAOS object id
 * 
 */
typedef union {
    u_int16_t val;
    struct {
        u_int8_t lsb;
        u_int8_t msb;
    } bytes;
} object_id_t;

/**
 * @brief BAOS ojbect count
 * 
 */
typedef union {
    u_int16_t val;
    struct {
        u_int8_t lsb;
        u_int8_t msb;
    } bytes;
} object_count_t;

/**
 * @brief BAOS server item. Used for all req/res involving the server
 * 
 */
struct baos_server_item {
    object_id_t id;
    byte length;
    byte *data;
};
typedef struct baos_server_item baos_server_item_t;

/**
 * @brief BAOS datapoint. Used for all req/res involving the KNX
 * 
 */
struct baos_datapoint {
    object_id_t id;
    union {
        byte state;
        byte command;
    };
    byte length;
    byte *data;
};
typedef struct baos_datapoint baos_datapoint_t;

/**
 * @brief BAOS frame. Used as data storage for communication with the kberry838.
 * 
 */
struct baos_frame {
    byte subservice;
    object_id_t first_obj_id;
    object_count_t obj_count;
    baos_frame_type type;
    byte error_code;

    union {
        baos_server_item_t **server_items;
        baos_datapoint_t **datapoints;
    };
};
typedef struct baos_frame baos_frame_t;

/**
 * @brief Store global BAOS client data
 * 
 */
struct baos_client {
    baos_frame_type response_type;
    baos_frame_t *response;
};

/**
 * @brief Send a getServerItemReq (see datasheet) and wait for a response
 * 
 * @param first_item First item to get 
 * @param item_count Number of items to get starting from first item
 * @return baos_frame_t* Kberry repsonse
 */
baos_frame_t *baos_get_server_item(u_int16_t first_item, u_int16_t item_count);

/**
 * @brief Store a response in the global struct
 * 
 * @param buf byte array of the BAOS frame
 * @param len byte array length
 */
void baos_store_response(byte *buf, int len);

/**
 * @brief Free a BAOS frame
 * 
 * @param frame frame to free
 */
void baos_free_frame(baos_frame_t *frame);

/**
 * @brief Read the value of datapoints
 * 
 * @param first_datapoint_id First datapoint id
 * @param datapoint_count Number of datapoint the get the value of. Starting from first id
 * @return baos_frame_t* BAOS frame containing the response
 */
baos_frame_t *baos_get_datapoint_value(u_int16_t first_datapoint_id, u_int16_t datapoint_count);

/**
 * @brief Set the value of a datapoints
 * 
 * @param datapoints Array of datapoint to set the value of.
 * @param datapoints_count Number of element in array
 */
void baos_set_datapoint_value(baos_datapoint_t *datapoints, int datapoints_count);

/**
 * @brief Print BAOS frame
 * 
 * @param frame frame to print
 */
void baos_print_frame(baos_frame_t *frame);

/**
 * @brief Subscribe to receive KNX events
 * 
 * @param indication_fn Callback function
 */
void baos_client_subscribe_to_indications(void (*indication_fn)(baos_frame_t *frame));

/**
 * @brief Read the description of datapoints
 * 
 * @param first_datapoint_id First datapoint id
 * @param datapoint_count Number of datapoint the get the value of. Starting from first id
 * @return baos_frame_t* BAOS frame containing the response
 */
baos_frame_t *baos_get_datapoint_description(u_int16_t first_datapoint_id, u_int16_t datapoint_count);
#endif
