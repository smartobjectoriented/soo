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
#include <soo/device/baos_client.h> 

#if 0
#define DEBUG
#endif

#if 0
#define DEBUG_THREAD
#endif

/* Access to serdev */
static struct kberry838_uart *kberry838;

/**
 * @brief Prints an array of bytes. Use for debug proposes.
 * 
 * @param buf Array of bytes
 * @param len Array length
 */
#ifdef DEBUG
static void kberry838_print_buffer(byte *buf, int len) {
    int i;

    dev_info(kberry838->dev, "Buffer length: %d\n",len);
    for (i = 0; i < len; i++) {
        dev_info(kberry838->dev, "[%d]: 0x%02X\n", i, buf[i]);
    }
}
#endif

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
        dev_err(kberry838->dev, "Not enough room. Length: %u\n", len);
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

/**
 * @brief Reset the kberry838 device internal registers and state of the stack
 * 
 */
static void kberry838_send_reset(void) {
    kberry838_write_buf(kberry838_reset, sizeof(kberry838_reset));

    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Acknowledge timeout reached");
        BUG();
    }
    dev_info(kberry838->dev, "Resetted successfully");
}

/**
 * @brief Acknowledge new data received.
 * 
 */
static void kberry838_send_ack(void) {
    kberry838_write_buf(kberry838_ack, sizeof(kberry838_ack));
}

/**
 * @brief Invert the frame parity.
 * 
 */
void kberry838_switch_frame_parity(void) {
    kberry838->even = kberry838->even ? false : true;
}

/**
 * @brief Convert a BAOS frame into a FT12 array of byte to be sent to the kberry838 device.
 * 
 * @param baos_frame Frame to encapsulate
 * @param len Number of bytes of the frame
 * @return byte* FT12 array of bytes
 */
static byte *kberry838_build_ft12_frame(byte *baos_frame, int len) {
    byte *ft12_frame;
    long ft12_checksum = 0;
    int i;

    ft12_frame = kzalloc((FT12_HEADER_SIZE + len + FT12_END_FRAME_SIZE) * sizeof(byte), GFP_KERNEL);
    BUG_ON(!ft12_frame);

    /** FT 1.2 Header **/
    ft12_frame[FT12_START_OFF] = FT12_START_CHAR;

    /** Account for FT12 Control field **/
    ft12_frame[FT12_LENGTH_OFF] = len + 1;
    ft12_frame[FT12_REPEAT_LENGTH_OFF] = len + 1;
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

/**
 * @brief Send a BAOS frame to the kberry838 device.
 * 
 * @param baos_frame Frame to send
 * @param len Number of byte of the frame
 */
void kberry838_send_data(byte *baos_frame, int len) {
    byte *ft12_array;
    int ft12_len;

    ft12_len = FT12_HEADER_SIZE + len + FT12_END_FRAME_SIZE;
    ft12_array = kberry838_build_ft12_frame(baos_frame, len);
    BUG_ON(!ft12_array);

    kberry838_write_buf(ft12_array, ft12_len);
    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Send data timeout reached");
        BUG();
    }

    kfree(ft12_array);

    // kthread_run(kberry838_send_data_th, args, "send_data_th");
}

/**
 * @brief Process incoming serial data and extract BAOS frame.
 * 
 * @param data Data received from the serial port.
 * @param len Length of the received data
 * @param frame_len return number of bytes in the frame
 * @param bytes_read return number of byte processed
 * @return byte* if the end frame byte is found return the BAOS frame, NULL otherwise
 */
static byte *kberry838_process_ft12_frame(const byte *data, size_t len, int *frame_len, int *bytes_read) {
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
                decode_status = GET_LENGTH;
                processed_bytes = 0;
            } 
            break;
        
        case GET_LENGTH:
            if (processed_bytes == 2) {
                BUG_ON(baos_frame_len != data[i]);
                decode_status = GET_SECOND_START;
            }
            else
                baos_frame_len = data[i];
            break;

        case GET_SECOND_START:
            if (data[i] != FT12_START_CHAR) {
                BUG();
            }
            decode_status = GET_CONTROL_FIELD;
            break;

        case GET_CONTROL_FIELD:
            ft12_control_field = data[i];
            if (kberry838->even) {
                if (ft12_control_field != FT12_EVEN_RSP) {
                    dev_err(kberry838->dev, "Control field is 0x%02X instead of 0x%02X",
                            ft12_control_field, FT12_EVEN_RSP);
                    BUG();
                }
            } else {
                if (ft12_control_field != FT12_ODD_RSP) {
                    dev_err(kberry838->dev, "Control field is 0x%02X instead of 0x%02X",
                            ft12_control_field, FT12_ODD_RSP);
                    BUG();
                }
            }

            ft12_checksum += ft12_control_field;
            /** Remove FT12 control field from length **/
            baos_frame_len -= 1;
            decode_status = GET_BAOS;
            if (baos_frame_len > 1) {
                baos_frame = kzalloc(baos_frame_len * sizeof(byte), GFP_KERNEL);
                BUG_ON(!baos_frame);
            }
            break;

        case GET_BAOS:
            if (processed_bytes == FT12_HEADER_SIZE + baos_frame_len) {
                decode_status = GET_STOP;
                ft12_checksum %= 0x100;
                /** Check if there errors **/
                if ((byte)ft12_checksum != data[i]) {
                    dev_err(kberry838->dev, "FT12 checksum is wrong. Calculated: 0x%02X, Read: 0x%02X",
                            (byte)ft12_checksum, data[i]);
                    kfree(baos_frame);
                    decode_status = GET_START;
                    *bytes_read += 1;
                    return NULL;
                }

            } else {
                baos_frame[processed_bytes - FT12_HEADER_SIZE] = data[i];
                ft12_checksum += data[i];
            }
            break;
        
        case GET_STOP:
            if (data[i] == FT12_STOP_CHAR) {
                *frame_len = baos_frame_len;
                baos_frame_len = 0;
                ft12_checksum = 0;
                ft12_control_field = 0;
                decode_status = GET_START;
                *bytes_read += 1;
                return baos_frame;
            }
            break;

        default:
            break;
        }
        processed_bytes++;
        *bytes_read += 1;
    }

    return NULL;
}

/**
 * @brief Callback called by serdev controller when new data is received
 * 
 * @param serdev 
 * @param buf 
 * @param len 
 * @return int 
 */
static int kberry838_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    byte *baos_frame = NULL;
    int baos_frame_len = 0;
    int bytes_processed = 0;

    while (bytes_processed < len) {
        /** Check if acknowledge **/
        if (buf[bytes_processed] == FT12_ACK) {
            dev_info(kberry838->dev, "ACK received");
            complete(&kberry838->wait_rsp);
            bytes_processed++;
            if (len == 1)
                break; 
        }
        /** Extract BAOS frame **/
        baos_frame = kberry838_process_ft12_frame(&buf[bytes_processed], len - bytes_processed,
                                                    &baos_frame_len, &bytes_processed);
        if (baos_frame) {
#ifdef DEBUG
            dev_info(kberry838->dev, "Received new BAOS frame");
#endif
            kberry838_send_ack();
            baos_store_response(baos_frame, baos_frame_len);
            kberry838_switch_frame_parity();
            kfree(baos_frame);
        }
    }
    return len;
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = kberry838_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

/**
 * @brief Read the firmware version to test communication with the kberry838 device.
 * 
 */
static void kberry838_init_server(void) {
    baos_frame_t *rsp;
    char *serial_num;
    int i;

    rsp = baos_get_server_item(BAOS_ID_SERIAL_NUM, 0x01);
    if (rsp) {
        if (rsp->server_items[0]) {
            serial_num = kzalloc(2 * rsp->server_items[0]->length * sizeof(char), GFP_KERNEL);
            BUG_ON(!serial_num);
            for (i = 0; i < rsp->server_items[0]->length; i++)
                sprintf(&serial_num[2 * i], "%02X", rsp->server_items[0]->data[i]);
            dev_info(kberry838->dev, "Kberry838 Serial number: %s\n", serial_num);
            baos_free_frame(rsp);
        }

    } else {
        dev_err(kberry838->dev, "Failed to read kberry838 serial number\n");
        BUG();
    }
}

#ifdef DEBUG_THREAD
/**
 * @brief Test thread. Send command DOWN -> STOP -> UP
 * 
 * @param data NULL 
 * @return int 0
 */
static int test_thread(void *data) {
    baos_frame_t *rsp;
    byte up_dn [2] = {0x00, 0x01};
    baos_datapoint_t blind[1] = { 
        {.id.val = 0x01, .command = SET_NEW_VALUE_AND_SEND_ON_BUS, sizeof(up_dn[1]), &up_dn[1]} 
    };

    /** Wait for boot to complete **/
    msleep(10000);

    /** Blind get position **/
    rsp = baos_get_datapoint_value(0x05, 0x01);
    baos_print_frame(rsp);
    if (rsp->first_obj_id.val == 0x05) {
        dev_info(kberry838->dev, "Blind postion: 0x%02X\n", rsp->datapoints[0]->data[0]);
    }
    baos_free_frame(rsp);
    msleep(1000);

    /** Blind down **/
    dev_info(kberry838->dev, "========= Send blind down ========");
    baos_set_datapoint_value(blind, 1);
    msleep(3000);

    /** Blind stop **/
    dev_info(kberry838->dev, "========= Send blind stop ========");
    blind[0].id.val = 0x02;
    baos_set_datapoint_value(blind, 1);
    msleep(8000);
    
    /** Blind get position **/
    rsp = baos_get_datapoint_value(0x05, 0x01);
    baos_print_frame(rsp);
    if (rsp->first_obj_id.val == 0x05) {
        dev_info(kberry838->dev, "Blind postion: 0x%02X\n", rsp->datapoints[0]->data[0]);
    }
    baos_free_frame(rsp);
    msleep(1000);

    /** Blind up **/
    dev_info(kberry838->dev, "========= Send blind up ========");
    blind[0].id.val = 0x01;
    blind[0].data = &up_dn[0];
    baos_set_datapoint_value(blind, 1);

    return 0;
}
#endif

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
    kberry838_init_server();

#ifdef DEBUG_THREAD
    kthread_run(test_thread, NULL, "test-thread");
#endif

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