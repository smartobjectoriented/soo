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
static void kberry838_print_buffer(byte *buf, int len) {
#ifdef DEBUG
    int i;

    dev_info(kberry838->dev, "Buffer length: %d\n",len);
    for (i = 0; i < len; i++) {
        dev_info(kberry838->dev, "[%d]: 0x%02X\n", i, buf[i]);
    }
#endif
}

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
    size_t i;

    BUG_ON(!kberry838->is_open);

    if (serdev_device_write_room(kberry838->serdev) < len) {
        dev_err(kberry838->dev, "Not enough room. Length: %lu\n", len);
        BUG();
    }

    for (i = 0; i < len; i++) {
        tx_bytes = serdev_device_write(kberry838->serdev, &buffer[i], sizeof(byte), msecs_to_jiffies(10));
        if (tx_bytes < 0)
            return -1;
        byte_written += tx_bytes;
    }

    return byte_written;
}

/**
 * @brief Reset the kberry838 device internal registers and state of the stack
 * 
 */
static int kberry838_send_reset_request(void) {
    kberry838_write_buf(kberry838_reset_req, sizeof(kberry838_reset_req));
    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Acknowledge timeout reached, device is not connected");
        return -1;
    }
    dev_info(kberry838->dev, "Resetted successfully");

    return 0;
}

/**
 * @brief Reset the kberry838 device internal registers and state of the stack
 * 
 */
void kberry838_send_reset_indication(void) {
    kberry838_write_buf(kberry838_reset_ind, sizeof(kberry838_reset_ind));
    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Acknowledge timeout reached");
        BUG();
    }
    dev_info(kberry838->dev, "Resetted indications successfully");
}

/**
 * @brief Acknowledge new data received.
 * 
 */
void kberry838_send_ack(void) {
    kberry838->decode_status = WAIT_FT12_START;
    kberry838_write_buf(kberry838_ack, sizeof(kberry838_ack));
    serdev_device_wait_until_sent(kberry838->serdev, 0);
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

    ft12_frame = kzalloc((FT12_HEADER_SIZE + len + FT12_FOOTER_SIZE) * sizeof(byte), GFP_KERNEL);
    BUG_ON(!ft12_frame);

    /** FT 1.2 Header **/
    ft12_frame[FT12_START_OFF] = FT12_START_CHAR;

    /** Account for FT12 Control field **/
    ft12_frame[FT12_LENGTH_OFF] = len + 1;
    ft12_frame[FT12_REPEAT_LENGTH_OFF] = len + 1;
    ft12_frame[FT12_REPEAT_START_OFF] = FT12_START_CHAR;
    ft12_frame[FT12_CONTROL_BYTE_OFF] = kberry838->request_cr;

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

    ft12_len = FT12_HEADER_SIZE + len + FT12_FOOTER_SIZE;
    ft12_array = kberry838_build_ft12_frame(baos_frame, len);
    BUG_ON(!ft12_array);

    kberry838->request_cr = kberry838->request_cr == FT12_EVEN_FRAME ? FT12_ODD_FRAME : FT12_EVEN_FRAME;
    kberry838->decode_status = WAIT_FT12_START;

    kberry838_write_buf(ft12_array, ft12_len);
    serdev_device_wait_until_sent(kberry838->serdev, 0);
    if (wait_for_completion_timeout(&kberry838->wait_rsp, 
                                    msecs_to_jiffies(KBERRY838_RSP_TIMEOUT)) == 0) {
        dev_err(kberry838->dev, "Send data timeout reached");
        BUG();
    }

    kfree(ft12_array);
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
static byte *kberry838_check_ft12_and_extract_baos(const byte *ft12buf, size_t ft12len, int baos_frame_len) {
    byte *baos_frame = NULL;
    long ft12_checksum = 0;

    int i;

    if (ft12buf[FT12_START_OFF] != FT12_START_CHAR) {
        dev_err(kberry838->dev, "Error: FT1.2 first char is 0x%02X instead of 0x%02X", 
                ft12buf[FT12_START_OFF], FT12_START_CHAR);
        return NULL;
    }
    
    if (ft12buf[FT12_LENGTH_OFF] != ft12buf[FT12_REPEAT_LENGTH_OFF]) {
        dev_err(kberry838->dev, "Error: FT1.2 length is wrong. [1] is 0x%02X, [2] is 0x%02X", 
                ft12buf[FT12_LENGTH_OFF], ft12buf[FT12_REPEAT_LENGTH_OFF]);
        return NULL;
    }

    if (ft12buf[FT12_REPEAT_START_OFF] != FT12_START_CHAR) {
        dev_err(kberry838->dev, "Error: FT1.2 second start is 0x%02X instead of 0x%02X", 
                ft12buf[FT12_REPEAT_START_OFF], FT12_START_CHAR);
        return NULL;
    }
    
    /** Check control byte **/
    if (ft12buf[FT12_CONTROL_BYTE_OFF] != kberry838->response_cr) {
        dev_err(kberry838->dev, "Error: FT1.2 CR byte is 0x%02X instead of 0x%02X", 
            ft12buf[FT12_CONTROL_BYTE_OFF], kberry838->response_cr);
        return NULL;
    }

    ft12_checksum += ft12buf[FT12_CONTROL_BYTE_OFF];

    baos_frame = kzalloc(baos_frame_len * sizeof(byte), GFP_KERNEL);
    BUG_ON(!baos_frame);

    memcpy(baos_frame, &ft12buf[FT12_START_BAOS_OFF], baos_frame_len);

    for (i = 0; i < baos_frame_len; i++)
        ft12_checksum += baos_frame[i];
    
    ft12_checksum %= 0x100;

    if (ft12buf[FT12_START_BAOS_OFF + baos_frame_len] != (byte)ft12_checksum) {
        dev_err(kberry838->dev, "Error: FT1.2 checksum byte is 0x%02X instead of 0x%02X", 
                (byte)ft12_checksum, ft12buf[FT12_START_BAOS_OFF + baos_frame_len]);
        kfree(baos_frame);
        return NULL;
    }

    if (ft12buf[FT12_START_BAOS_OFF + baos_frame_len + 1] != FT12_STOP_CHAR) {
        dev_err(kberry838->dev, "Error: FT1.2 last byte is 0x%02X instead of 0x%02X", 
                ft12buf[FT12_START_BAOS_OFF + baos_frame_len + 1], FT12_STOP_CHAR );
        kfree(baos_frame);
        return NULL;
    }
    
    return baos_frame;
}

static void kberry838_process_ft12_frame(const byte *data, size_t len){
    static byte* ft12_frame;
    static int ft12_len = 0;
    static bool start_found = false;
    static int fbyte_proc = 0;
    byte *baos_frame;
    int baos_frame_len;
    int i;
    
    for (i = 0; i < len; i++) {
        if (data[i] == FT12_ACK && kberry838->decode_status == WAIT_FT12_START) {
#ifdef DEBUG
            dev_info(kberry838->dev, "ACK received");
#endif        
            complete(&kberry838->wait_rsp);  
        } else {
            switch(kberry838->decode_status) {
                
                case WAIT_FT12_START:
                    if (data[i] == FT12_START_CHAR && !start_found) {
                        start_found = true;
                        fbyte_proc = 0;
                    } else if (start_found) {
                        start_found = false;
                        ft12_len = FT12_HEADER_SIZE + data[i] + 1;

                        ft12_frame = kzalloc(ft12_len * sizeof(byte), GFP_KERNEL);
                        BUG_ON(!ft12_frame);
                        memset(ft12_frame, 0, ft12_len * sizeof(byte));

                        ft12_frame[fbyte_proc++] = FT12_START_CHAR;
                        ft12_frame[fbyte_proc++] = data[i];
                        kberry838->decode_status = WAIT_FT12_STOP;
                    } else  {
                        dev_err(kberry838->dev, "Don't know what to do with this value. Dropping it!");
                        kberry838_print_buffer((byte*)data, len);
                    }
                    break;

                case WAIT_FT12_STOP:
                    if (data[i] != FT12_STOP_CHAR)
                        ft12_frame[fbyte_proc++] = data[i];
                    else {
                        ft12_frame[fbyte_proc++] = data[i];
                        
                        baos_frame_len = ft12_len - FT12_HEADER_SIZE - FT12_FOOTER_SIZE;
                        baos_frame = kberry838_check_ft12_and_extract_baos(ft12_frame, ft12_len, 
                                                                            baos_frame_len);

                        kberry838->response_cr = kberry838->response_cr == FT12_EVEN_RSP ? FT12_ODD_RSP : FT12_EVEN_RSP; 
                        
                        if (baos_frame) {
                            kberry838_send_ack();
                            baos_store_response(baos_frame, baos_frame_len);
                            kfree(baos_frame);
                        } else {
                            dev_err(kberry838->dev, "Failed to extract baos frame from FT1.2 data");
                        }

                        kberry838->decode_status = WAIT_FT12_START;
                    }
                    break;
            }
        }
    }
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
    kberry838_process_ft12_frame(buf, len);
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

    rsp = baos_get_server_item(BAOS_PROTOCOL_VERS, 0x01);
    if (rsp) {
        if (rsp->server_items[0]) {
            serial_num = kzalloc(2 * rsp->server_items[0]->length * sizeof(char), GFP_KERNEL);
            BUG_ON(!serial_num);
            for (i = 0; i < rsp->server_items[0]->length; i++)
                sprintf(&serial_num[2 * i], "%02X", rsp->server_items[0]->data[i]);
            dev_info(kberry838->dev, "Kberry838 protocol version: %s\n", serial_num);
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

static int kberry_delayed_probe_fn(void *data) {

    int ret = 0;

    /* TODO: Change this method!!!! */
    /* Add a delay to "wait" for the serdev to be ready */ 
    msleep(8000);

    ret = serdev_device_open(kberry838->serdev);
    if (ret < 0) {
        dev_err(kberry838->dev, "Failed to open serial port\n");
        BUG();
    }
    kberry838->is_open = 1;

    ret = serdev_device_set_baudrate(kberry838->serdev, kberry838->baud);
    if (ret != kberry838->baud) {
        dev_err(kberry838->dev, "Failed to set baudrate\n");
        BUG();
    }

    ret = serdev_device_set_parity(kberry838->serdev, SERDEV_PARITY_EVEN);

    serdev_device_set_flow_control(kberry838->serdev, false);

    if (kberry838_send_reset_request() < 0) {
        dev_err(kberry838->dev, "Failed to reset\n");
        return -1;
    }
    
    msleep(1000);
    
    kberry838_init_server();

    dev_info(kberry838->dev, "Delayed probe successful!\n");

    return 0;
}

static int kberry838_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
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
    kberry838->decode_status = WAIT_FT12_START;
    kberry838->request_cr = FT12_ODD_FRAME;
    kberry838->response_cr = FT12_ODD_RSP;
    
    init_completion(&kberry838->wait_rsp);

    /* The rest of the init is done in a separate thread to allow serdev to be fully probed */
    kthread_run(kberry_delayed_probe_fn, NULL, "kberry-delayed-probe");

#ifdef DEBUG_THREAD
    kthread_run(test_thread, NULL, "test-thread");
#endif

    dev_info(dev,"Probed 1st stage successfully\n");

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
