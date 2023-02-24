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
#include <soo/device/mipot32001353.h>
#include <linux/string.h>
#include <linux/kthread.h>

#if 1
#define DEBUG
#endif


/* Access to serdev */
static struct mipot320_uart *mipot320;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(byte *data);

static mipot_msg_t reset = {RESET, 0, NULL};
static mipot_msg_t get_fw_vers = {GET_FW_VERSION, 0, NULL};

/**
 * @brief Write synchronously to serial port.
 * 
 * @param buf data to write
 * @param len data size in bytes
 * @return int byte written
 */
int mipot320_write_buf(const byte *buffer, size_t len) {
    int tx_bytes = 0, byte_written = 0;
    size_t i;

    BUG_ON(!mipot320->is_open);

    if (serdev_device_write_room(mipot320->serdev) < len) {
        dev_err(mipot320->dev, "Not enough room\n");
        return -1;
    }

    /** Write the entire buffer. If the operation can't be done in 
     *  all at once, repeat the write operation until all bytes have
     *  been written.
     */
    // while(tx_bytes < len) {
    //     tx_bytes = serdev_device_write(mipot320->serdev, &buffer[byte_written], len - byte_written, 0);
    //     BUG_ON(tx_bytes < 1);
    //     byte_written += tx_bytes;
    // }
    for (i = 0; i < len; i++) {
        tx_bytes = serdev_device_write(mipot320->serdev, &buffer[i], sizeof(byte), msecs_to_jiffies(10));
        if (tx_bytes < 0)
            return -1;
        byte_written += tx_bytes;
    }

    return byte_written;
}

/**
 * @brief Send a command from cmd_list to the RN2483 device
 * 
 * @param cmd command to send
 * @param args command arguments if any, else NULL
 */
static int mipot320_send_msg(mipot_msg_t msg) {
    byte *buf;
    int crc = 0;
    size_t i, len = msg.length + MIPOT320_MIN_MSG_LEN;

    buf = kzalloc(sizeof(byte) * len, GFP_KERNEL);
    BUG_ON(!buf);

    buf[MIPOT320_HEADER_OFFS] = MIPOT320_HEADER;
    buf[MIPOT320_CMD_OFFS] = (byte)msg.cmd;
    buf[MIPOT320_LEN_OFFS] = msg.length;

    if (msg.length) {
        memcpy(&buf[MIPOT320_DATA_OFFS], msg.payload, msg.length * sizeof(byte));
    }
    
    for (i = 0; i < len - 1; i++) {
        crc = (crc + buf[i]) & 0xFF;
    }

    buf[MIPOT320_DATA_OFFS + msg.length] = 0x100 - (byte)crc;

    mipot320->current_cmd = msg.cmd;
    
#ifdef DEBUG
    dev_info(mipot320->dev, "Sending msg:");
    for (i = 0; i < len; i++) {
        dev_info(mipot320->dev, "[%lu]: 0x%02X", i, buf[i]);
    }
#endif

    if (mipot320_write_buf(buf, len) == 0) {
        dev_err(mipot320->dev, "Failed to send message");
    }

    if (wait_for_completion_interruptible_timeout(&mipot320->wait_rsp, msecs_to_jiffies(500)) == 0) {
        dev_err(mipot320->dev, "Response timeout reached, something went wrong");
        return -ENODEV;
    }

    kfree(buf);
    return 0;
}

void mipot_send_data(char *data, int len) {
    /** TODO: implement send data **/
}

/**
 * @brief Process received data from serial. 
 * 
 * @param buf received buffer
 * @param len buffer length
 * @param prev_data if the previous call did not signal data_end == 1 put the previous data else NULL
 * @param data_len total length of the data. It makes sense only when data_end == 1
 * @param data_end signals that all the data has been received data_end == 1 else data_end == 0
 * @return byte* data received
 */
static byte *process_received_buffer(const byte *buf, size_t len, byte *prev_data, int *data_len, bool *data_end) {
    static int prev_data_len;
    byte *data;
    int i;

    *data_end = 0;

    /** Reset **/
    if (!prev_data)
        prev_data_len = 0;

    /** Store current received data length **/
    if (prev_data_len)
        *data_len = prev_data_len;

    /** Search end of message \r\n **/
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            *data_end = 1;
            prev_data_len += i;
            break;
        }
    }

    /** Needs to get more data **/
    if (!(*data_end)) {
        prev_data_len += len;
    }

    data = kzalloc((prev_data_len) * sizeof(byte), GFP_KERNEL);
    BUG_ON(!data);
    
    /** If data was already received copy old data on to expanded array**/
    if (prev_data) {
        memcpy(data, prev_data, prev_data_len);
        memcpy(&data[*data_len], buf, prev_data_len - *data_len);
        kfree(prev_data);
    } else {
        memcpy(data, buf, prev_data_len);
    }

    if (*data_end)
        data[prev_data_len - 1] = '\0';

    *data_len = prev_data_len;

    return data;
}

/**
 * @brief Callback for serdev when new data is received
 * 
 * @param serdev serial device 
 * @param buf data received
 * @param len data size in bytes
 * @return int data received size
 */
static int mipot320_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    int msg_len, i;
    bool timeout = false, msg_end;
    static byte *msg = NULL;

#ifdef DEBUG
    dev_info(mipot320->dev, "New data received:");
    for (i = 0; i < len; i++) {
        dev_info(mipot320->dev, "[%d]: 0x%02X\n", i, buf[i]);
    }
#endif

    complete(&mipot320->wait_rsp);

    return len;
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = mipot320_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

int mipot320_subscribe(void (*callback)(byte *data)) {
    if (!callback)
        return -1;

    if (subscribers_count >= MAX_SUBSCRIBERS) {
        dev_err(mipot320->dev, "Subscriber limit reached");
        return -1;
    }

    dev_info(mipot320->dev, "New subscriber registered");

    subscribers[subscribers_count++] = callback;

    return 0;
}

static int mipot320_delayed_probe_fn(void *data) {
    /** Wait for boot to complete **/
    int ret = 0;

    msleep(10000);

    ret = serdev_device_open(mipot320->serdev);
    if (ret < 0) {
        dev_err(mipot320->dev, "Failed to open serial port\n");
        return ret;
    }
    mipot320->is_open = 1;

    ret = serdev_device_set_baudrate(mipot320->serdev, 115200);
    if (ret != 115200) {
        dev_err(mipot320->dev, "Failed to set baudrate\n");
        return -1;
    }

    ret = serdev_device_set_parity(mipot320->serdev, SERDEV_PARITY_NONE);
    if (ret < 0) {
        dev_err(mipot320->dev, "Failed to set parity\n");
        return ret;
    }

    ret = serdev_device_set_rts(mipot320->serdev, false);
    if (ret < 0) {
        dev_err(mipot320->dev, "Failed to set rts/cts\n");
        return ret;
    }

    serdev_device_set_flow_control(mipot320->serdev, false);

    if (mipot320_send_msg(reset) < 0) {
        dev_info(mipot320->dev, "Failed to reset");
    }

    msleep(1000);

    if (mipot320_send_msg(get_fw_vers) < 0) {
        dev_info(mipot320->dev, "Failed to get fw version");
    }

    serdev_device_close(mipot320->serdev);

    return 0;
}

static int mipot320_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
    u32 baud;

    dev = &serdev->dev;
    BUG_ON(!dev);

    mipot320 = kzalloc(sizeof(struct mipot320_uart), GFP_KERNEL);
    BUG_ON(!mipot320);

    /* Check if baudrate is defined in the dts, else use default */
    if (of_property_read_u32(dev->of_node, "current-speed", &baud) < 0) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
        baud = MIPOT320_SERDEV_DEFAULT_BAUDRATE;
    }
    serdev_device_set_client_ops(serdev, &uart_serdev_device_ops);

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    mipot320->serdev = serdev;
    mipot320->dev = dev;
    mipot320->baud = baud;
    init_completion(&mipot320->wait_rsp);
    
    kthread_run(mipot320_delayed_probe_fn, NULL, "mipot-delayed-probe-th");  

    dev_info(dev,"Probed successfully\n");

    return 0;
}

static void mipot320_serdev_remove(struct serdev_device *serdev) {
    serdev_device_close(serdev);
    kfree(mipot320);
}

static const struct of_device_id mipot320_serdev_dt_ids[] = {
	{ .compatible = MIPOT320_COMPATIBLE },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rn2483_serdev_dt_ids);

static struct serdev_device_driver mipot320_serdev_drv = {
    .probe = mipot320_serdev_probe,
    .remove = mipot320_serdev_remove,
    .driver = {
        .name       = MIPOT320_SERDEV_NAME,
        .of_match_table = of_match_ptr(mipot320_serdev_dt_ids),
    },
};
module_serdev_device_driver(mipot320_serdev_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
MODULE_DESCRIPTION("LoRa uart driver");
