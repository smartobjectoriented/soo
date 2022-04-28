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
#include <soo/device/rn2483.h>


/* Access to serdev */
static struct rn2483_uart *rn2483;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(void *data);

/** Response functions **/
static int rn2483_process_cmd_response(byte *data)
{
    switch (rn2483->current_cmd)
    {
    case reset:
        if (strcmp(data, RN2483_INVALID_PARAM) == 0) {
            dev_err(rn2483->dev, "Failed to reset device: %s", data);
            BUG();
        }
        dev_info(rn2483->dev, "Reset successful: %s", data);
        rn2483->status = IDLE;
        break;

    case radio_rx:
        if (strcmp(data, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to set listening mode: %s", data);
            BUG();
        }
        rn2483->status = LISTEN;
        break;

    case stop_rx:
        if (strcmp(data, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to stop listening: %s", data);
            BUG();
        }
        dev_info(rn2483->dev, "Stopped listening for radio messages");
        rn2483->status = IDLE;
        break;

    case mac_pause:
        if (strcmp(data, "0") == 0) {
            dev_err(rn2483->dev, "Failed to pause lora mac");
            BUG();
        }
        dev_info(rn2483->dev, "Lora mac paused for %s ms", data);
        rn2483->status = IDLE;
        break;

    default:
        break;
    }

    return 0;
}

/**
 * @brief Write synchronously to serial port.
 * 
 * @param buf data to write
 * @param len data size in bytes
 * @return int byte written
 */
int rn2483_write_buf(const byte *buffer, size_t len) {
    int tx_bytes = 0;
    int byte_written = 0;

    BUG_ON(!rn2483->is_open);

    if (serdev_device_write_room(rn2483->serdev) < len) {
        dev_err(rn2483->dev, "Not enough room\n");
        BUG();
    }

    /** Write the entire buffer. If the operation can't be done in 
     *  all at once, repeat the write operation until all bytes have
     *  been written.
     */
    while(tx_bytes < len) {
        tx_bytes = serdev_device_write_buf(rn2483->serdev, &buffer[byte_written], len - byte_written);
        BUG_ON(tx_bytes < 1);
        byte_written += tx_bytes;
    }

    /** Add <CR><LN> to signal the end of the message **/
    tx_bytes = serdev_device_write_buf(rn2483->serdev, "\r\n", 2);
    BUG_ON(tx_bytes != 2);
    byte_written += tx_bytes;

    serdev_device_write_flush(rn2483->serdev);

#ifdef DEBUG
    dev_info(tcm515->dev, "byte written: %d\n", tx_bytes);
#endif

    return byte_written;
}

void rn2483_send_cmd(rn2483_cmd_t cmd, char *args) {
    rn2483->status = SEND_CMD;
    rn2483->current_cmd = cmd;

    rn2483_write_buf(cmd_list[cmd], strlen(cmd_list[cmd]));

    dev_info(rn2483->dev, "Successfully sent command: %s", cmd_list[cmd]);
}

static byte *process_data(byte *buf, size_t len, byte *prev_data, int *data_len, int *data_end) {
    static int _data_len;
    byte *data;
    int i;

    *data_end = 0;

    /** Reset **/
    if (!prev_data)
        _data_len = 0;

    /** Store current received data length **/
    if (_data_len)
        *data_len = _data_len;

    /** Search end of message \r\n **/
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            *data_end = 1;
            _data_len += i;
            break;
        }
    }

    /** Needs to get more data **/
    if (!(*data_end)) {
        _data_len += len;
    }

    data = kzalloc((_data_len) * sizeof(byte), GFP_KERNEL);
    
    /** If data was already received copy old data **/
    if (prev_data) {
        memcpy(data, prev_data, _data_len);
        memcpy(&data[*data_len], buf, _data_len - *data_len);
    } else {
        memcpy(&data, buf, _data_len);
    }

    kfree(prev_data);

    *data_len = _data_len;

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
static int rn2483_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    int msg_len, msg_end;
    static byte *msg = NULL;

    dev_info(rn2483->dev, "New data received: %s\n", buf);

    msg = process_data(buf, len, msg, &msg_len, &msg_end);

    if (msg_end) {
        /** Do somenthing with the msg **/
        switch (rn2483->status) {
            case SEND_CMD:
                /** call process cmd response **/
                rn2483_process_cmd_response(msg);
                break;
            case SEND_MSG:
                /** call process send msg response **/
                break;
            case LISTEN:
                /** process listen **/
                break;
            case IDLE:
                /** Dropping data **/
                dev_info(rn2483->dev, "Dont know what to do with data received: %s", msg);
                break;
        }
        /** Reset for next msg **/
        kfree(msg);
        msg = NULL;
    }

    return len;
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = rn2483_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

static int rn2483_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
    int ret = 0;
    u32 baud;

    dev = &serdev->dev;
    BUG_ON(!dev);

    rn2483 = kzalloc(sizeof(struct rn2483_uart), GFP_KERNEL);
    BUG_ON(!rn2483);

    /* Check if baudrate is defined in the dts, else use default */
    if (of_property_read_u32(dev->of_node, "current-speed", &baud) < 0) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
        baud = RN2483_SERDEV_DEFAULT_BAUDRATE;
    }
    serdev_device_set_client_ops(serdev, &uart_serdev_device_ops);

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    rn2483->serdev = serdev;
    rn2483->dev = dev;
    rn2483->baud = baud;

    ret = serdev_device_open(serdev);
    if (ret < 0) {
        dev_err(dev, "Failed to open serial port\n");
        BUG();
    }
    rn2483->is_open = 1;

    ret = serdev_device_set_baudrate(serdev, baud);
    if (ret != baud) {
        dev_err(dev, "Failed to set baudrate\n");
        BUG();
    }

    rn2483_send_cmd(reset, NULL);
    rn2483_send_cmd(mac_pause, NULL);

    /** Listen for data **/
    rn2483_send_cmd(radio_rx, "0");

    dev_info(dev,"Probed successfully\n");

    return 0;
}

static void rn2483_serdev_remove(struct serdev_device *serdev) {
    serdev_device_close(serdev);
    kfree(rn2483);
}

static const struct of_device_id rn2483_serdev_dt_ids[] = {
	{ .compatible = RN2483_COMPATIBLE },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rn2483_serdev_dt_ids);

static struct serdev_device_driver rn2483_serdev_drv = {
    .probe = rn2483_serdev_probe,
    .remove = rn2483_serdev_remove,
    .driver = {
        .name       = RN2483_SERDEV_NAME,
        .of_match_table = of_match_ptr(rn2483_serdev_dt_ids),
    },
};
module_serdev_device_driver(rn2483_serdev_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
MODULE_DESCRIPTION("LoRa uart driver");