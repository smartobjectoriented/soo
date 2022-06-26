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

#if 0
#define CONFIG_PRINTK
#endif

#if 0
#define DEBUG
#endif

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soo/device/tcm515.h>
#include <linux/serdev.h>
#include <linux/jiffies.h>
#include <soo/uapi/console.h>

/*** Commands packets ***/
static esp3_packet_t co_rd_version_packet = {
    .header = {0x01, 0x00, COMMON_COMMAND},
    .data = (byte[]){CO_RD_VERSION},
    .optional_data = NULL,
};

/* Access to serdev */
static struct tcm515_uart *tcm515;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(esp3_packet_t *packet);

/**
 * @brief Process the response obtained by sending the command CO_RD_VERSION
 *        which return basic informations of the TCM515 device
 * 
 * @param packet packet received
 */
void tcm515_read_id(esp3_packet_t *packet) {
    enum read_id_fsm state = GET_APP_VERS;
    byte app_vers[APP_VERS_SIZE];
    byte api_vers[API_VERS_SIZE];
    byte chip_id[CHIP_ID_SIZE];
    byte chip_vers[CHIP_VERS_SIZE];
    byte app_desc[APP_DESC_SIZE];
    byte data;
    size_t i, j = 0, byte_read = 0;
    
    if (packet->data[0] != ESP3_RET_OK)
        BUG();

    for (i = 1; i < packet->header.data_len; i++) {
        data = packet->data[i];
        switch(state) {
            case GET_APP_VERS:
                app_vers[j++] = data;
                if (j == APP_VERS_SIZE) {
                    j = 0;
                    state = GET_API_VERS;
                }
                break;
            
            case GET_API_VERS:
                api_vers[j++] = data;
                if (j == API_VERS_SIZE) {
                    j = 0;
                    state = GET_CHIP_ID;
                }
                break;
            
            case GET_CHIP_ID:
                chip_id[j++] = data;
                if (j == CHIP_ID_SIZE) {
                    j = 0;
                    state = GET_CHIP_VERS;
                }
                break;

            case GET_CHIP_VERS:
                chip_vers[j++] = data;
                if (j == CHIP_VERS_SIZE) {
                    j = 0;
                    state = GET_APP_DESC;
                }
                break;

            case GET_APP_DESC:
                app_desc[j++] = data;
                if (j == APP_DESC_SIZE) {
                    j = 0;
                    break;
                }
        }
        byte_read++;
    }

    printk(TCM515_SERDEV_PREFIX "Infos :\n");
    printk("\tAPP version: 0x");
    for (i = 0; i < APP_VERS_SIZE; i++)
        printk(KERN_CONT "%02X", app_vers[i]);

    printk("\tAPI version: 0x");
    for (i = 0; i < API_VERS_SIZE; i++)
        printk(KERN_CONT "%02X", api_vers[i]);

    printk("\tChip ID: 0x");
    for (i = 0; i < CHIP_ID_SIZE; i++)
        printk(KERN_CONT "%02X", chip_id[i]);

    printk("\tChip version: 0x");
    for (i = 0; i < CHIP_VERS_SIZE; i++)
        printk(KERN_CONT "%02X", chip_vers[i]);

    printk("\tAPP description: ");
    printk(KERN_CONT "%s\n", app_desc);
}

/**
 * @brief Write synchronously to serial port.
 * 
 * @param buf data to write
 * @param len data size in bytes
 * @return int byte written
 */
int tcm515_write_buf(const byte *buffer, size_t len, bool expect_resp, 
                        void (*response_fn)(esp3_packet_t *packet)) {
    int tx_bytes = 0;
    int byte_written = 0;

    BUG_ON(!tcm515->is_open);

    if (expect_resp) {
        BUG_ON(!response_fn);

        tcm515->expect_response = 1;
        tcm515->response_fn = response_fn;
    }

    if (serdev_device_write_room(tcm515->serdev) < len) {
        dev_err(tcm515->dev, "Not enough room\n");
        BUG();
    }

    /** Write the entire buffer. If the operation can't be done in 
     *  all at once, repeat the write operation until all bytes have
     *  been written.
     */
    while(tx_bytes < len) {
        tx_bytes = serdev_device_write_buf(tcm515->serdev, &buffer[byte_written], len - byte_written);
        BUG_ON(tx_bytes < 1);
        byte_written += tx_bytes;
    }
    serdev_device_write_flush(tcm515->serdev);

#ifdef DEBUG
    dev_info(tcm515->dev, "byte written: %d\n", tx_bytes);
#endif

    return byte_written;
}

int tcm515_subscribe(void (*callback)(esp3_packet_t *packet)) {
    if (!callback)
        return -1;

    if (subscribers_count >= MAX_SUBSCRIBERS)
        return -1;

    /** We use printk cause this function can be called before probe */
    printk(TCM515_SERDEV_PREFIX "New subscriber registered\n");

    subscribers[subscribers_count++] = callback;
    return 0;
}

/**
 * @brief Callback for serdev when new data is received
 * 
 * @param serdev serial device 
 * @param buf data received
 * @param len data size in bytes
 * @return int data received size
 */
static int tcm515_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len) {
    size_t i, j;
    read_status read;
    esp3_packet_t *packet;

    for (i = 0; i < len; i++) {
        read = esp3_read_byte(buf[i], &packet);

        switch(read) {
            case READ_ERROR:
                dev_err(tcm515->dev, "ESP3 packet contained errors. Dropping it\n");
                break;

            case READ_END:
#ifdef DEBUG
                dev_info(tcm515->dev, "New ESP3 packet received\n");
                esp3_print_packet(packet);
#endif
                BUG_ON(!packet);

                switch(packet->header.packet_type) {
                    case RADIO_ERP1:
                        /* Broadcast to all subscribers */
                        for (j = 0; j < subscribers_count; j++) {
                            if (subscribers[j])
                                (*subscribers[j])(packet);
                        }
                        break;
                    
                    case RESPONSE:
                        if (tcm515->expect_response) {
                            BUG_ON(!tcm515->response_fn);

                            tcm515->response_fn(packet);
                            /* Reset for next time */
                            tcm515->response_fn = NULL;
                            tcm515->expect_response = 0;
                        } else {
                            dev_info(tcm515->dev, "No response function was set.\n");
                        }
                        break;
                    default:
                        dev_info(tcm515->dev, "Packet type %d not yet implemented\n", packet->header.packet_type);
                        break;
                }

                esp3_free_packet(packet);
                break;

            case READ_PROGRESS:
                /* Do nothing */
                break;
        }
    }

    return len;
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = tcm515_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

static int tcm515_serdev_probe(struct serdev_device *serdev) {
    struct device *dev;
    byte *get_id_version;
    int ret = 0;
    u32 baud;

    dev = &serdev->dev;
    BUG_ON(!dev);

    tcm515 = kzalloc(sizeof(struct tcm515_uart), GFP_KERNEL);
    BUG_ON(!tcm515);

    /* Check if baudrate is defined in the dts, else use default */
    if (of_property_read_u32(dev->of_node, "current-speed", &baud) < 0) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
        baud = TCM515_SERDEV_DEFAULT_BAUDRATE;
    }
    serdev_device_set_client_ops(serdev, &uart_serdev_device_ops);

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    tcm515->serdev = serdev;
    tcm515->dev = dev;
    tcm515->baud = baud;
    tcm515->expect_response = 0;
    tcm515->response_fn = NULL;

    ret = serdev_device_open(serdev);
    if (ret < 0) {
        dev_err(dev, "Failed to open serial port\n");
        BUG();
    }
    tcm515->is_open = 1;

    ret = serdev_device_set_baudrate(serdev, baud);
    if (ret != baud) {
        dev_err(dev, "Failed to set baudrate\n");
        BUG();
    }

    get_id_version = esp3_packet_to_byte_buffer(&co_rd_version_packet);
    if (get_id_version) {
        tcm515_write_buf(get_id_version, CO_READ_VERSION_BUFFER_SIZE, true, tcm515_read_id);
        dev_info(dev,"Probed successfully\n");
        return 0;
    } else {
        dev_err(dev, "Probe failed\n");
        return -1;
    }
}

static void tcm515_serdev_remove(struct serdev_device *serdev) {
    serdev_device_close(serdev);

    kfree(tcm515);
}

static const struct of_device_id tcm515_serdev_dt_ids[] = {
	{ .compatible = TCM515_COMPATIBLE },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tcm515_serdev_dt_ids);

static struct serdev_device_driver tcm515_serdev_drv = {
    .probe = tcm515_serdev_probe,
    .remove = tcm515_serdev_remove,
    .driver = {
        .name       = TCM515_SERDEV_NAME,
        .of_match_table = of_match_ptr(tcm515_serdev_dt_ids),
    },
};
module_serdev_device_driver(tcm515_serdev_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
MODULE_DESCRIPTION("En0cean uart driver");
