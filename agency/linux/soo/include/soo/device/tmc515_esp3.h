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

/*** ESP3 packet format ***/
/* Doc: https://usermanual.wiki/m/a0b4d9036aad0f4f220621c1d89bad843cbb72e96b17194c9248bb519fc3b2bc.pdf */
/*

            Synchro byte    |  Header                                           |   CRC8H       |   Data    |   Optional data   |   CRC8D       |
    offset      0x0         |   0x1,0x2     |   0x3             |   0x4         |   0x5         |   0x6     |   0x6 + x         |   0x6 + x + y |
                
                0x55        |   Data length |   Optional length |   Packet type |   Header CRC8 |   data    |   additional data |   Data CRC8   |

*/
#ifndef _ESP3_PROTOCOL_H
#define _ESP3_PROTOCOL_H

#include <linux/types.h>

/*** ESP3 packet bytes offsets ***/
#define SYNC_BYTE_OFFSET                0x0
#define DATA_LENGTH_OFFSET              0x1
#define OPTIONAL_LENGTH_OFFSET          0x3
#define PACKET_TYPE_OFFSET              0x4
#define CRC8H_OFFSET                    0x5
#define DATA_OFFSET                     0x6

/*** ESP3 Header offsets ***/
#define HEADER_DATA_LEN_OFFSET          0x00
#define HEADER_OPT_LEN_OFFSET           0x02
#define HEADER_TYPE_OFFSET              0x03

/*** ESP3 Constants ***/
#define ESP3_SYNC_BYTE                  0x55
#define ESP3_HEADER_SIZE                0x04
#define ESP3_FIX_SIZE                   0x03

/*** ESP3 Common commands ***/
#define CO_WR_SLEEP                     0x01
#define CO_WR_RESET                     0x02
#define CO_RD_VERSION                   0x03

/***ESP3 cmd buffer ***/
#define CO_READ_VERSION_BUFFER_SIZE     0x08
#define CO_READ_VERSION                 0x03

#define ESP3_RET_OK                     0x00

#define BUFFER_MAX_SIZE                 65535

#define DATA_LEN_STR_SIZE               0x05 /* Values plus '\0' */
#define OPT_LEN_STR_SIZE                0x03 /* Values plus '\0' */

typedef unsigned char byte;

/**
 * @brief Finite state machine used to decode an ESP3 packet
 * 
 */
enum esp3_fsm {
    GET_SYNC_BYTE,
    GET_HEADER,
    GET_CRC8H,
    GET_DATA,
    GET_OPTIONAL,
    GET_CRC8D
};
typedef enum esp3_fsm esp3_fsm;

/**
 * @brief Current status of packet decoding
 * 
 */
enum read_status {
    READ_ERROR,
    READ_END,
    READ_PROGRESS,
};
typedef enum read_status read_status;

/**
 * @brief ESP3 packet types
 * 
 */
enum esp3_packet_type{
    RADIO_ERP1 = 1,
    RESPONSE,
    RADIO_SUBTEL,
    EVENT,
    COMMON_COMMAND,
    SMART_ACK_COMMAND,
    REMOTE_MAN_COMMAND,
};
typedef enum esp3_packet_type esp3_packet_type;

/**
 * @brief ESP3 header struct
 * 
 */
struct esp3_header {
    u16 data_len;
    byte optional_len;
    esp3_packet_type packet_type;
};
typedef struct esp3_header esp3_header_t;

/**
 * @brief ESP3 packet struct
 * 
 */
struct esp3_packet {
    esp3_header_t header;
    byte *data;
    byte *optional_data;
};
typedef struct esp3_packet esp3_packet_t;

/**
 * @brief Read a received byte and moves through the FSM.
 * 
 * @param buf New data byte
 * @param packet packet in which to store the received data. Is valid if read_status = READ_END
 * @return read_status 
 */
read_status esp3_read_byte(const byte buf, esp3_packet_t **packet);

/**
 * @brief Prints to stdout a packet
 * 
 * @param pck packet to print
 */
void esp3_print_packet(esp3_packet_t *pck);

/**
 * @brief Free allocated memory
 * 
 * @param pck packet to free
 */
void esp3_free_packet(esp3_packet_t *pck);

/**
 * @brief Converts a packet to a byte buffer to be sended by serial. Remember to free 
 *          buffer once sent.
 * 
 * @param packet Packet to get the byte buffer of
 * @return byte* buffer on success, NULL on error.
 */
byte *esp3_packet_to_byte_buffer(esp3_packet_t *packet);

#endif //_ESP3_PROTOCOL_H