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

#if 1
#define CONFIG_PRINTK
#endif

#if 0
#define DEBUG
#endif

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/tcm515_serdev.h>
#include <linux/serdev.h>
#include <linux/jiffies.h>

struct tcm515_serdev *tcm515;

/* Number of subscriber to tcm515 */
static int subscribers_count = 0;

/* Array of subscribers callback funtions */
static void (*subscribers[MAX_SUBSCRIBERS])(esp3_packet_t *packet);

/**
 * @brief Open serial port
 * 
 * @return int 0 on success, -1 on error
 */
int tcm515_open(void)
{
    int ret = 0;

    if (tcm515->is_open) {
        return 0;
    }

    if ((ret = serdev_device_open(tcm515->serdev)) < 0) {
        dev_err(tcm515->dev, "Failed to open uart");
        return ret;
    }

    tcm515->is_open = 1;
 
    return 0;
}

/**
 * @brief Close serial port
 * 
 * @return int 0 on success, -1 on error
 */
int tcm515_close(void)
{
    int ret = 0;

    if (tcm515->is_open) {
        serdev_device_close(tcm515->serdev);
        tcm515->is_open = 0;
    }

    return ret;
}

/**
 * @brief Set serial port baudrate
 * 
 * @param baud baudrate
 * @return int 0 on success, -1 on error
 */
int tcm515_set_baudrate(int baud)
{
    int ret = 0;

    if (tcm515->is_open) {
        if (serdev_device_set_baudrate(tcm515->serdev, baud) != baud) {
            dev_err(tcm515->dev ,"Failed to set baudrate\n");
            ret = -EINVAL;
        } else {
            tcm515->baud = baud;
        }
    } else {
        dev_err(tcm515->dev, "Failed to set baudrate. Port is closed\n");
        ret = -1;
    }

    return ret;
}

/**
 * @brief Set serial port parity
 * 
 * @param parity Parity
 * @return int 0 on success, -1 on error
 */
int tcm515_set_parity(enum serdev_parity parity)
{
    int ret = 0;

    if (tcm515->is_open) {
        ret = serdev_device_set_parity(tcm515->serdev, parity);
        if (ret < 0) {
            dev_err(tcm515->dev, "Failed to set parity\n");
        }
    } else {
        dev_err(tcm515->dev, "Failed to set parity. Port is closed\n");
        ret = -1;
    }

    return ret;
}

/**
 * @brief Set serial port flow control
 * 
 * @param flow activate flow control
 * @return int 0 on success, -1 on error
 */
int tcm515_set_flow_control(bool flow)
{
    int ret = 0;

    if (tcm515->is_open) {
        serdev_device_set_flow_control(tcm515->serdev, flow);
    } else {
        dev_err(tcm515->dev, "Failed to set flow control. Port is closed\n");
        ret = -1;
    }

    return ret;
}

/**
 * @brief Set serial rts/cts
 * 
 * @param rts activate rts
 * @return int 0 on success, -1 on error
 */
int tcm515_set_rts(bool rts)
{
    int ret = 0;

    if (tcm515->is_open) {
        if ((ret = serdev_device_set_rts(tcm515->serdev, rts)) < 0) {
            dev_err(tcm515->dev, "Failed to set rts\n");
        }
    } else {
        dev_err(tcm515->dev, "Failed to set rts. Port is closed\n");
        ret = -1;
    }

    return ret;
}

/**
 * @brief Write synchronously to serial port.
 * 
 * @param buf data to write
 * @param len data size in bytes
 * @return int 0 on success, -1 on error
 */
int tcm515_write_buf(const unsigned char* buf, size_t len)
{
    int tx_bytes = 0;

    if (tcm515->is_open) {
        while(tx_bytes < len) {
            tx_bytes += serdev_device_write_buf(tcm515->serdev, buf, len);
            if (tx_bytes < 1) {
                dev_err(tcm515->dev, "Failed to write byte. Errno: %d\n", tx_bytes);
                break;
            }
        }
        serdev_device_wait_until_sent(tcm515->serdev, _msecs_to_jiffies(10));

#ifdef DEBUG
        dev_info(tcm515->dev, "byte written: %d\n", tx_bytes);
#endif

        return 0;
    } else {
        dev_err(tcm515->dev, "Failed to write buffer. Port is closed\n");
        return -1;
    }
}

int tcm515_subscribe(void (*callback)(esp3_packet_t *packet))
{
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
static int tcm515_serdev_receive_buf(struct serdev_device *serdev, const byte *buf, size_t len)
{
    size_t i, j;
    read_status read;
    esp3_packet_t *packet;

    for (i = 0; i < len; i++) {
        read = esp3_read_byte(buf[i]);
        switch(read) {
            case READ_ERROR:
                dev_err(tcm515->dev, "ESP3 packet contained errors. Dropping it\n");
                break;
            case READ_END:
#ifdef DEBUG
                dev_info(tcm515->dev, "New ESP3 packet received\n");
#endif
                packet = esp3_get_packet();
                if (packet) {
                    for (j = 0; j < subscribers_count; j++) {
                        if (subscribers[j])
                            (*subscribers[j])(packet);
                    }
                } else {
                    dev_err(tcm515->dev, "Failed to get ESP3 packet\n");
                }
#ifdef DEBUG
                esp3_print_packet(packet);
#endif
                esp3_free_packet(packet);
                break;
            default:
                break;
        }
    }

    return len;
}

static void tcm515_serdev_write_wake_up(struct serdev_device *serdev)
{
    if (tcm515->serdev != serdev) {
        dev_err(tcm515->dev, "serdev don't match\n");
        return;
    }
}

static const struct serdev_device_ops uart_serdev_device_ops = {
    .receive_buf = tcm515_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

static int tcm515_serdev_probe(struct serdev_device *serdev)
{
    struct device *dev = &serdev->dev;
    byte *get_id_version;
    int ret = -1;
    u32 baud;

    tcm515 = kzalloc(sizeof(struct tcm515_serdev), GFP_KERNEL);
    if (!tcm515) {
        dev_err(dev, "Failed to allocate struct enocean uart");
        ret = -ENOMEM;
        goto alloc_tcm515_err;
    }

    if (of_property_read_u32(dev->of_node, "current-speed", &baud) < 0) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
        baud = TCM515_SERDEV_DEFAULT_BAUDRATE;
    }
    serdev_device_set_client_ops(serdev, &uart_serdev_device_ops);

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    tcm515->serdev = serdev;
    tcm515->dev = &serdev->dev;
    tcm515->is_open = 0;

    if ((ret = tcm515_open()) < 0)
        goto tcm515_open_err;

    if ((ret = tcm515_set_baudrate(baud)) < 0)
        goto tcm515_settings_err;

    if ((ret = tcm515_set_parity(SERDEV_PARITY_NONE)) < 0)
        goto tcm515_settings_err;

    if ((ret = tcm515_set_flow_control(TCM515_SERDEV_DEFAULT_FLOW_CTRL)) < 0)
        goto tcm515_settings_err;

    if ((ret = tcm515_set_rts(TCM515_SERDEV_DEFAULT_RTS)) < 0)
        goto tcm515_settings_err;

    /** TODO: Find why this is not working. See amba-pl011 register write **/
    /* get_id_version = esp3_packet_to_byte_buffer(&co_rd_version_packet);
    if (get_id_version) {
        tcm515_write_buf(get_id_version, CO_READ_VERSION_BUFFER_SIZE);
    } */
    
    dev_info(dev,"Probed successfully\n");
    ret = 0;

    return ret;
    
    tcm515_settings_err:
        tcm515_close();
    tcm515_open_err:
        kfree(tcm515);   
    alloc_tcm515_err:
        return ret;
}

static void tcm515_serdev_remove(struct serdev_device *serdev)
{
    if (tcm515_close() < 0) {
        dev_err(tcm515->dev, "Failed to close serial port\n");
    }

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