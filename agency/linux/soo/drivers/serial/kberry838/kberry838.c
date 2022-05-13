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
#include <linux/module.h>
#include <linux/of.h>
#include <soo/device/tcm515.h>
#include <linux/serdev.h>
#include <linux/jiffies.h>
#include <soo/uapi/console.h>
#include <soo/device/kberry838.h>
#include <linux/string.h>
#include <linux/kthread.h>

#include "baos_client.h"

#if 0
#define DEBUG
#endif

#if 0
#define DEBUG_THREAD
#endif



/* Access to serdev */
static struct kberry838_uart *kberry838;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(byte *data);

/**
 * @brief Write synchronously to serial port.
 * 
 * @param buf data to write
 * @param len data size in bytes
 * @return int byte written
 */
int kberry838_write_buf(const byte *buffer, size_t len) {
    int tx_bytes = 0;
    int byte_written = 0;

    BUG_ON(!kberry838->is_open);

    if (serdev_device_write_room(kberry838->serdev) < len) {
        dev_err(kberry838->dev, "Not enough room\n");
        BUG();
    }

    /** Write the entire buffer. If the operation can't be done in 
     *  all at once, repeat the write operation until all bytes have
     *  been written.
     */
    while(tx_bytes < len) {
        tx_bytes = serdev_device_write_buf(kberry838->serdev, &buffer[byte_written], len - byte_written);
        BUG_ON(tx_bytes < 1);
        byte_written += tx_bytes;
    }

    byte_written += tx_bytes;

    serdev_device_write_flush(kberry838->serdev);

    return byte_written;
}

static void kberry838_send_reset(void) {
    kberry838->status = SEND_RST;
    kberry838_write_buf(kberry838_reset, sizeof(kberry838_reset));

    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Acknowledge timeout reached");
        BUG();
    }
}

static void kberry838_send_ack(void) {
    kberry838_write_buf(kberry838_ack, sizeof(kberry838_ack));
}


static byte *kberry838_build_ft12_frame(byte *baos_frame, int len) {
    byte *ft12_frame;
    long ft12_checksum = 0;
    int i;

    ft12_frame = kzalloc((FT12_HEADER_SIZE + len + FT12_END_FRAME_SIZE) * sizeof(byte), GFP_KERNEL);
    BUG_ON(!ft12_frame);

    /** FT 1.2 Header **/
    ft12_frame[FT12_START_OFF] = FT12_START_CHAR;
    ft12_frame[FT12_LENGTH_OFF] = len;
    ft12_frame[FT12_REPEAT_LENGTH_OFF] = len;
    ft12_frame[FT12_REPEAT_START_OFF] = FT12_START_CHAR;
    ft12_frame[FT12_CONTROL_BYTE_OFF] = kberry838->even ? FT12_EVEN_FRAME : FT12_ODD_FRAME;

    ft12_checksum += ft12_frame[FT12_CONTROL_BYTE_OFF];

    /** Add BAOS frame **/
    for (i = 0; i < len; i++) {
        ft12_frame[FT12_START_BAOS_OFF + i] = baos_frame[i];
        ft12_checksum += baos_frame[i];
    }

    /** Modulo 256 **/
    ft12_checksum %= 0x100;
    
    ft12_frame[FT12_START_BAOS_OFF + len] = (byte)ft12_checksum;
    ft12_frame[FT12_START_BAOS_OFF + len + 1] = FT12_STOP_CHAR;

    return ft12_frame;
}

void kberry838_send_data(byte *baos_frame, int len) {
    byte *buf;
    int i;
    int buf_len;

    buf_len = FT12_HEADER_SIZE + len + FT12_END_FRAME_SIZE;

    kberry838->status = SEND_MSG;

    buf = kberry838_build_ft12_frame(baos_frame, len);

    dev_info(kberry838->dev, "Send data:\n");
    for (i = 0; i < buf_len; i++) {
        dev_info(kberry838->dev, "[%d]: 0x%02X\n", i, buf[i]);
    }

    kberry838_write_buf(buf, buf_len);
    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Send data timeout reached");
        BUG();
    }

    if (kberry838->even)
        kberry838->even = false;
    else
        kberry838->even = true;
}

static byte *kberry838_process_ft12_frame(const byte *data, size_t len) {
    static baos_decode_status_t decode_status = GET_START;
    static int processed_bytes = 0;
    static byte *baos_frame = NULL;
    static int baos_frame_len = 0, ft12_control_field = 0;
    static long ft12_checksum = 0;
    int i;

    for (i = 0; i < len; i++) {
        switch (decode_status)
        {
        case GET_START:
            if (data[i] == FT12_START_CHAR) {
                if (processed_bytes > 2)
                    decode_status = GET_CONTROL_FIELD;
                else 
                    decode_status = GET_LENGTH;
            }
            break;
        
        case GET_LENGTH:
            if (processed_bytes > 1) {
                BUG_ON(baos_frame_len != data[i]);
                decode_status = GET_START;
            }
            else
                baos_frame_len = data[i];
            break;

        case GET_CONTROL_FIELD:
            ft12_control_field = data[i];
            decode_status = GET_BAOS;
            baos_frame = kzalloc(baos_frame_len * sizeof(byte), GFP_KERNEL);
            BUG_ON(!baos_frame);
            break;

        case GET_BAOS:
            if (processed_bytes > FT12_HEADER_SIZE + baos_frame_len) {
                decode_status = GET_STOP;
                ft12_checksum %= 0x100;
                if ((byte)ft12_checksum != data[i]) {
                    dev_err(kberry838->dev, "FT12 checksum is wrong. Calculated: 0x%02X, Read: 0x%02X",
                            (byte)ft12_checksum, data[i]);
                    kfree(baos_frame);
                    decode_status = GET_START;
                    return NULL;
                }

            } else {
                baos_frame[processed_bytes - FT12_HEADER_SIZE] = data[i];
                ft12_checksum += data[i];
            }
            break;
        
        case GET_STOP:
            if (data[i] == FT12_STOP_CHAR) {
                processed_bytes = 0;
                baos_frame_len = 0;
                ft12_checksum = 0;
                ft12_control_field = 0;
                decode_status = GET_START;

                return baos_frame;
            }
            break;

        default:
            break;
        }
        processed_bytes++;
    }

    return NULL;
}

static int kberry838_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    int i;
    byte *baos_frame;

    dev_info(kberry838->dev, "New data received:\n");
    for (i = 0; i < len; i++)
        dev_info(kberry838->dev, "[%d]: 0x%02X\n", i, buf[i]);

    switch (kberry838->status) {
        case SEND_RST:
            if (buf[0] != KBERRY838_ACK) {
                dev_err(kberry838->dev, "Failed to reset kberry838 module");
                BUG();
            } 
            dev_info(kberry838->dev, "Successfully resetted kberry838 module");
            complete(&kberry838->wait_rsp);
            kberry838->status = IDLE;
            break;

        case SEND_MSG:
           if (buf[0] != KBERRY838_ACK) {
                dev_err(kberry838->dev, "Failed to send message to kberry838 module");
                BUG();
            } 
            kberry838->status = WAIT_DATA;
            break;

        case WAIT_DATA:
            baos_frame = kberry838_process_ft12_frame(buf, len);
            if (baos_frame) {
                complete(&kberry838->wait_rsp);
                kberry838_send_ack();
                kberry838->status = IDLE;
                kfree(baos_frame);
            }
            break;

        case IDLE:
            dev_err(kberry838->dev, "Don't know what to do with recevived data. Dropping it.");
            dev_err(kberry838->dev, "Data received:\n");
            for (i = 0; i < len; i++)
                dev_err(kberry838->dev, "[%d]: 0x%02X\n", i, buf[i]);
            break;
    }

    return len;
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = kberry838_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

int kberry838_subscribe(void (*callback)(byte *data)) {
    if (!callback)
        return -1;

    if (subscribers_count >= MAX_SUBSCRIBERS) {
        dev_err(kberry838->dev, "Subsciber limit reached");
        return -1;
    }

    dev_info(kberry838->dev, "New subscriber registered");

    subscribers[subscribers_count++] = callback;

    return 0;
}

static int kberry838_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
    int ret = 0;
    u32 baud;

    dev = &serdev->dev;
    BUG_ON(!dev);

    kberry838 = kzalloc(sizeof(struct kberry838_uart), GFP_KERNEL);
    BUG_ON(!kberry838);

    /* Check if baudrate is defined in the dts, else use default */
    if (of_property_read_u32(dev->of_node, "current-speed", &baud) < 0) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
        baud = KBERRY838_SERDEV_DEFAULT_BAUDRATE;
    }
    serdev_device_set_client_ops(serdev, &uart_serdev_device_ops);

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    kberry838->serdev = serdev;
    kberry838->dev = dev;
    kberry838->baud = baud;
    kberry838->status = IDLE;
    init_completion(&kberry838->wait_rsp);

    ret = serdev_device_open(serdev);
    if (ret < 0) {
        dev_err(dev, "Failed to open serial port\n");
        BUG();
    }
    kberry838->is_open = 1;

    ret = serdev_device_set_baudrate(serdev, baud);
    if (ret != baud) {
        dev_err(dev, "Failed to set baudrate\n");
        BUG();
    }

    ret = serdev_device_set_parity(serdev, SERDEV_PARITY_EVEN);
    if (ret < 0) {
        dev_err(dev, "Failed to set parity\n");
        BUG();
    }

    kberry838_send_reset();
    kberry838->even = false;

    baos_get_server_item(BAOS_ID_SERIAL_NUM, 0x01);

    dev_info(dev,"Probed successfully\n");

    return 0;
}

static void kberry838_serdev_remove(struct serdev_device *serdev) {
    serdev_device_close(serdev);
    kfree(kberry838);
}

static const struct of_device_id kberry838_serdev_dt_ids[] = {
	{ .compatible = KBERRY838_COMPATIBLE },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, kberry838_serdev_dt_ids);

static struct serdev_device_driver kberry838_serdev_drv = {
    .probe = kberry838_serdev_probe,
    .remove = kberry838_serdev_remove,
    .driver = {
        .name       = KBERRY838_SERDEV_NAME,
        .of_match_table = of_match_ptr(kberry838_serdev_dt_ids),
    },
};
module_serdev_device_driver(kberry838_serdev_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
MODULE_DESCRIPTION("KNX uart driver");