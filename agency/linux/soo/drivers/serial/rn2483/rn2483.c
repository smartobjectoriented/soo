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
#include <soo/device/rn2483.h>
#include <linux/string.h>
#include <linux/kthread.h>

#if 1
#define DEBUG
#endif

#if 0
#define DEBUG_THREAD
#endif

/* Access to serdev */
static struct rn2483_uart *rn2483;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(byte *data);

/** Response functions **/

/**
 * @brief Process RN2483 response to commands
 * 
 * @param rsp Response received from the RN2483
 */
static void rn2483_process_cmd_response(byte *rsp) {
    switch (rn2483->current_cmd)
    {
    case RESET:
        if (strcmp(rsp, RN2483_INVALID_PARAM) == 0) {
            dev_err(rn2483->dev, "Failed to reset device: %s", rsp);
            BUG();
        }
        dev_info(rn2483->dev, "Reset successful: %s", rsp);
        rn2483->status = IDLE;
        break;

    case RADIO_RX:
        if (strcmp(rsp, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to set listening mode: %s", rsp);
            BUG();
        }
        dev_info(rn2483->dev, "Start listening for radio messages");
        rn2483->status = LISTEN;
        break;

    case STOP_RX:
        if (strcmp(rsp, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to stop listening: %s", rsp);
            BUG();
        }
        dev_info(rn2483->dev, "Stopped listening for radio messages");
        rn2483->status = IDLE;
        break;

    case MAC_PAUSE:
        if (strcmp(rsp, "0") == 0) {
            dev_err(rn2483->dev, "Failed to pause lora mac");
            BUG();
        }
        dev_info(rn2483->dev, "Lora mac paused for %s ms", rsp);
        rn2483->status = IDLE;
        break;

    case SET_WDT:
        if (strcmp(rsp, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to set watchdog: %s", rsp);
            BUG();
        }
        dev_info(rn2483->dev, "Watchdog setted successfully");
        rn2483->status = IDLE;
        break;

    case GET_VERSION:
        if (strcmp(rsp, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to get version: %s", rsp);
            BUG();
        }
        dev_info(rn2483->dev, "Version: %s", rsp);
        rn2483->status = IDLE;
        break;

    default:
        dev_err(rn2483->dev, "Not supported command: %s --> %s", cmd_list[rn2483->current_cmd], rsp);
        BUG();
        break;
    }
}

/**
 * @brief Process send message RN2483 response
 * 
 * @param rsp Response received from RN2483
 */
static void process_send_msg_response(byte *rsp) {
    static bool transmit_pending = false;

    /** 
     * The rn2483 sends a RN2483_OK when it has received the serial data and 
     * a RN2483_RADIO_TX_OK when data has been effectively sent through LoRa.
     * The send_data function needs 2 response to be considered has completed.
     */ 
    if (transmit_pending) {
        if (strcmp(rsp, RN2483_RADIO_TX_OK) != 0) {
            dev_err(rn2483->dev, "Failed to send message: %s", rsp);
            BUG();
        }
        complete(&rn2483->wait_rsp);
        transmit_pending = false;
    } else {
        if (strcmp(rsp, RN2483_OK) != 0) {
            dev_err(rn2483->dev, "Failed to send message: %s", rsp);
            BUG();
        }
        transmit_pending = true;
    }    
}

/**
 * @brief Process received LoRa data and forward it to all subscribers
 * 
 * @param data Received data
 * @param timeout if mac pause ends timeout is set to true else is false
 */
static void rn2483_process_listen(byte *data, bool *timeout) {
    byte *msg;
    char *token;
    char num[3] = {0};
    char delim = DELIM_CHAR;
    int msg_len, valid_msg = 0, i, error = 0;
    long tmp_val;

#ifdef DEBUG
    dev_info(rn2483->dev, "New radio data received: %s", data);
#endif

    *timeout = false;
    /** Check if timeout is expired **/
    if (strcmp(data, RN2483_RADIO_ERR) == 0) {
        *timeout = true;
        dev_info(rn2483->dev, "Radio rx timeout reached");
    }

    /** Separate radio_rx from data **/
    do {
        token = strsep((char**)&data, &delim);

        if(strcmp(token, RN2483_RADIO_RX) == 0) {
            valid_msg = 1;
            continue;
        }

        /** Parse data string 2 by 2**/
        if (valid_msg && (strlen(token) > 0)) {
            msg_len = strlen(token) / 2;
            msg = kzalloc(msg_len, GFP_KERNEL);
            
            for (i = 0; i < msg_len; i++) {
                memcpy(num, &token[2 * i], 2);

                /** Error in data **/
                if (kstrtol(num, 16, &tmp_val) < 0) {
                    error = 1;
                    break;
                }

                if (tmp_val < 0xFF)
                    msg[i] = (byte)tmp_val;
                else
                    msg[i] = 0;

            }
            break;
        }
    } while(token != NULL);

    if (!valid_msg) {
        dev_err(rn2483->dev, "Received data is not a radio_rx: %s", data);
        BUG();
    }

    if (error) {
        dev_err(rn2483->dev, "Error found in data received. Dropping it");
        kfree(msg);
    }

    msg_len = strlen(msg);
    dev_info(rn2483->dev, "Data received: %s", msg);

    /** send data to subscribers **/
    for (i = 0; i < subscribers_count; i++)
        subscribers[i](msg);

    kfree(msg);
    rn2483->status = IDLE;
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

    return byte_written;
}

/**
 * @brief Send a command from cmd_list to the RN2483 device
 * 
 * @param cmd command to send
 * @param args command arguments if any, else NULL
 */
static int rn2483_send_cmd(rn2483_cmd_t cmd, char *args) {
    byte *cmd_str;
    rn2483->status = SEND_CMD;
    rn2483->current_cmd = cmd;

    if (args) {
        cmd_str = kzalloc((strlen(cmd_list[cmd]) + strlen(args)) * sizeof(byte), GFP_KERNEL);
        BUG_ON(!cmd_str);

        sprintf(cmd_str, "%s %s", cmd_list[cmd], args);
        rn2483_write_buf(cmd_str, strlen(cmd_str));
#ifdef DEBUG
        dev_info(rn2483->dev, "Successfully sent command: %s", cmd_str);
#endif
    } else {
        rn2483_write_buf(cmd_list[cmd], strlen(cmd_list[cmd]));
#ifdef DEBUG
        dev_info(rn2483->dev, "Successfully sent command: %s", cmd_list[cmd]);
#endif
    }

    if (wait_for_completion_timeout(&rn2483->wait_rsp, msecs_to_jiffies(1000)) == 0) {
        dev_err(rn2483->dev, "Failed to execute %s. Timeout expired", __func__);
        return -1;
    }
    else
        return 0;
}

/**
 * @brief Set the RN2483 into continuos listening mode. To exit continuos listening mode
 *  send "rxstop" command.
 * 
 * @param args Timeout value
 * @return int 0
 */
static int rn2483_start_listening(void *args) {
    bool timeout = false;

    if (args) {
        timeout = *(bool*)(args);
    }

    if (timeout)
        rn2483_send_cmd(MAC_PAUSE, NULL);

    rn2483_send_cmd(RADIO_RX, "0");

    return 0;
}

void rn2483_send_data(char *data, int len) {
    byte *msg; 
    char raw_bytes[3];
    int msg_len, i;
    rn2483_send_cmd(STOP_RX, NULL);

    rn2483->status = SEND_MSG;
    rn2483->current_cmd = RADIO_TX;

    if (!data)
        BUG();

    msg_len = strlen(cmd_list[RADIO_TX]) + len * 2 + 2;
    msg = kzalloc(msg_len * sizeof(byte), GFP_KERNEL);
    BUG_ON(!msg);

    sprintf(msg, "%s ", cmd_list[RADIO_TX]);

    for (i = 0; i < len; i++) {
        snprintf(raw_bytes, 3, "%02X", data[i]);
        strcat(msg, raw_bytes);
    }

#ifdef DEBUG
    dev_info(rn2483->dev, "Sending msg [%d]: %s", msg_len, msg);
#endif

    rn2483_write_buf(msg, msg_len);
    wait_for_completion(&rn2483->wait_rsp);

    rn2483_start_listening(NULL);
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
static int rn2483_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    int msg_len;
    bool timeout = false, msg_end;
    static byte *msg = NULL;

#ifdef DEBUG
    dev_info(rn2483->dev, "New data received: %s", buf);
#endif
    msg = process_received_buffer(buf, len, msg, &msg_len, &msg_end);

    if (msg_end) {
#ifdef DEBUG
        dev_info(rn2483->dev, "New msg received: %s", msg);
#endif
        switch (rn2483->status) {
            case SEND_CMD:
                /** call process cmd response **/
                rn2483_process_cmd_response(msg);
                complete(&rn2483->wait_rsp);
                break;
            case SEND_MSG:
                /** call process send msg response **/
                process_send_msg_response(msg);
                break;
            case LISTEN:
                /** process listen **/
                rn2483_process_listen(msg, &timeout);
                kthread_run(rn2483_start_listening, (void*)(&timeout), "start_listen_th");
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

int rn2483_subscribe(void (*callback)(byte *data)) {
    if (!callback)
        return -1;

    if (subscribers_count >= MAX_SUBSCRIBERS) {
        dev_err(rn2483->dev, "Subsciber limit reached");
        return -1;
    }

    dev_info(rn2483->dev, "New subscriber registered");

    subscribers[subscribers_count++] = callback;

    return 0;
}

#if 0
/**
 * @brief This functions is used for developpement. Activate DEBUG_THREAD to use it. 
 * It sends a message every 5 seconds
 * 
 * @param args NULL
 * @return int 0
 */
static int rn2483_send_data_test(void *args) {
    int counter = 0;
    byte data [20];

    while(!kthread_should_stop()) {
        msleep(5000);
        sprintf(data, "Hello %d", counter++);
        rn2483_send_data(data, strlen(data));
    }

    return 0;
}
#endif

static int rn2483_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
    int ret = 0;
    u32 baud;
    int timeout = true;

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
    init_completion(&rn2483->wait_rsp);

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
    
    if (rn2483_send_cmd(RESET, NULL) < 0)
        return -1;
    
    /** Disable watchdog **/
    if (rn2483_send_cmd(SET_WDT, "0") < 0)
        return -1;

    /** Read version **/
    if (rn2483_send_cmd(GET_VERSION, NULL) < 0)
        return -1;

    /** Listen for data **/
    rn2483_start_listening(&timeout);

#ifdef DEBUG_THREAD
    kthread_run(rn2483_send_data_test, NULL, "send-data-test");
#endif

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
