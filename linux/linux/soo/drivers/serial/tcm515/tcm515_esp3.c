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

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/kernel.h>

#include <soo/device/tmc515_esp3.h>

#define proccrc8(u8CRC, u8Data) (u8CRC8Table[u8CRC ^ u8Data]);

byte u8CRC8Table[256] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
    0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
    0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
    0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
    0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
    0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
    0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
    0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
    0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
    0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
    0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
    0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
    0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
    0x76, 0x71, 0x78, 0x7f, 0x6A, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
    0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
    0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8D, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
    0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};

/**
 * @brief Calculate CRC8 for a sequence of bytes
 * 
 * @param buf bytes to calculate the CRC8 of
 * @param len bytes array length
 * @return byte CRC8 value
 */
byte esp3_calc_crc8(const byte *buf, size_t len) {
    int i;
    byte crc8_calc = 0;

    for (i = 0; i < len; i++) {
        crc8_calc = proccrc8(crc8_calc, buf[i]);
    }

    return crc8_calc;
}

/**
 * @brief Check CRC8 
 * 
 * @param buf bytes to calculate the CRC8 of
 * @param len bytes array length
 * @param crc8 CRC8 value to compare the result with
 * @return int 0 on success, -1 on error
 */
int esp3_check_crc8(const byte *buf, size_t len, byte crc8) {
    byte crc8_calc;

    crc8_calc = esp3_calc_crc8(buf, len);

    if (crc8_calc == crc8) {
        return 0;
    } else {
        pr_info("Calculated CRC8: 0x%02X, Received CRC8: 0x%02X\n", crc8_calc, crc8);
        return -1;
    }
}

read_status esp3_read_byte(const byte buf, esp3_packet_t **packet) {
    static esp3_fsm state = GET_SYNC_BYTE;
    static read_status ret = READ_PROGRESS;
    static int data_len = 0, optional_len = 0;
    
    /* Data buffer to store current data byte in */
    static byte data_buffer[BUFFER_MAX_SIZE];

    /* Current data header */
    static byte header[ESP3_HEADER_SIZE];
    
    static int i = 0;
    int crc8h, crc8d;

    switch (state)
    {
    case GET_SYNC_BYTE:
        /* Wait for the sync byte byte (0x55) to start processing the data */ 
        ret = READ_PROGRESS;
        if (buf == ESP3_SYNC_BYTE) {
            /*** reset all values for the new packet ***/
            memset(header, 0, ESP3_HEADER_SIZE);
            memset(data_buffer, 0, BUFFER_MAX_SIZE);
            state = GET_HEADER;
            optional_len = 0;
            data_len = 0;
            i = 0;
        }
        break;

    case GET_HEADER:
        /* First 4 bytes of data are the header */
        header[i++] = buf;
        if (i == ESP3_HEADER_SIZE) {
            state = GET_CRC8H;
            i = 0;
        }
        break;

    case GET_CRC8H:
        /* 5th byte is the CRC8 calculated using the 4 bytes of the header */
        crc8h = buf;
        /* Check if header is correct */
        if (esp3_check_crc8(header, ESP3_HEADER_SIZE, crc8h) < 0) {
            /* error detected wait for next sync byte */
            state = GET_SYNC_BYTE;
            ret = READ_ERROR;
        } else {
            data_len |= (header[HEADER_DATA_LEN_OFFSET] << 8) + header[HEADER_DATA_LEN_OFFSET + 1];
            optional_len = header[HEADER_OPT_LEN_OFFSET];

            state = GET_DATA;
        }
        break;

    case GET_DATA:
        /* Read data bytes until data length is reached */
        data_buffer[i++] = buf;
        if (i == data_len) {
            /* end of data */
            if (optional_len > 0)
                state = GET_OPTIONAL;
            else    
                state = GET_CRC8D;
        }
        break;

    case GET_OPTIONAL:
        /* Read optional data until optional data length is reached */
        data_buffer[i++] = buf;
        if (i == data_len + optional_len) {
            /* end of optional data */
            state = GET_CRC8D;
        }
        break;

    case GET_CRC8D:
        /* Last byte is the CRC8 calculated using the data and optional data */
        crc8d = buf;
        /* Check if data is correct */
        if (esp3_check_crc8(data_buffer, data_len + optional_len, crc8d) < 0) {
            ret = READ_ERROR;
        } else {
            /* Build a new ESP3 packet with the received data */
            *packet = kzalloc(sizeof(esp3_packet_t), GFP_KERNEL);
            BUG_ON(!*packet);

            (*packet)->header.data_len = (u16)data_len;
            (*packet)->header.optional_len = (byte)optional_len;
            (*packet)->header.packet_type = header[HEADER_TYPE_OFFSET];

            (*packet)->data = kzalloc(data_len * sizeof(byte), GFP_KERNEL);
            BUG_ON(!(*packet)->data);

            memcpy((*packet)->data, data_buffer, data_len);

            if(optional_len) {
                (*packet)->optional_data = kzalloc(optional_len * sizeof(byte), GFP_KERNEL);
                BUG_ON(!(*packet)->optional_data);

                memcpy((*packet)->optional_data, &data_buffer[data_len], optional_len);
            } else 
                (*packet)->optional_data = NULL;
            
            /* end of packet reached */
            ret = READ_END;
        }
        state = GET_SYNC_BYTE;
        break;

    default:
        break;
    }

    return ret;
}

void esp3_free_packet(esp3_packet_t *pck) {
    if (!pck)
        return;
        
    if (pck->header.data_len > 0) {
        kfree(pck->data);
    }

    if (pck->header.optional_len > 0) {
        kfree(pck->optional_data);
    }

    kfree(pck);
    pck = NULL;
}

byte *esp3_packet_to_byte_buffer(esp3_packet_t *packet) {
    byte *buf;
    int i;
    int buffer_size;

    buffer_size = 
        ESP3_HEADER_SIZE + packet->header.data_len + packet->header.optional_len + ESP3_FIX_SIZE;

    buf = kzalloc(buffer_size * sizeof(byte), GFP_KERNEL);
    if (!buf) {
        return NULL;
    }

    buf[SYNC_BYTE_OFFSET] = ESP3_SYNC_BYTE;
    buf[DATA_LENGTH_OFFSET] = packet->header.data_len >> 1;
    buf[DATA_LENGTH_OFFSET + 1] = (byte)packet->header.data_len;
    buf[OPTIONAL_LENGTH_OFFSET] = packet->header.optional_len;
    buf[PACKET_TYPE_OFFSET] = packet->header.packet_type;
    buf[CRC8H_OFFSET] = esp3_calc_crc8(&buf[DATA_LENGTH_OFFSET], ESP3_HEADER_SIZE);
    
    /*** get data ***/
    for (i = 0; i < packet->header.data_len; i++) {
        buf[DATA_OFFSET + i] = packet->data[i];
    }

    for (i = 0; i < packet->header.optional_len; i++) {
        buf[DATA_OFFSET + packet->header.data_len + i] = packet->optional_data[i];
    }

    buf[DATA_OFFSET + packet->header.data_len + packet->header.optional_len] = 
        esp3_calc_crc8(&buf[DATA_OFFSET], packet->header.data_len + packet->header.optional_len);

    return buf;
}

void esp3_print_packet(esp3_packet_t *pck) {
    int i;
    int data_len, opt_data_len;

    if (!pck)
        return;
    
    data_len = pck->header.data_len;
    opt_data_len = pck->header.optional_len;

    pr_info("ESP3 packet content:\n");
    pr_info("Data len: %d\n", pck->header.data_len);
    pr_info("Optional len: %d\n", pck->header.optional_len);
    pr_info("Packet type: 0x%02X\n", pck->header.packet_type);

    pr_info("Data:\n");
    if (data_len > 0) {
        for (i = 0; i < data_len; i++) {
            pr_info("[%d]: 0x%02X\n", i, pck->data[i]);
        }
    }

    pr_info("Optional data:\n");
    if (opt_data_len > 0) {
        for (i = 0; i < opt_data_len; i++) {
            pr_info("[%d]: 0x%02X\n", i, pck->optional_data[i]);
        }
    }
}
