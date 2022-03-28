#if 1
#define CONFIG_PRINTK
#endif

#include <linux/atomic.h>
#include <linux/crc-ccitt.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/sched.h>
#include <linux/serdev.h>
#include <asm/unaligned.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/enocean_uart.h>

#define ENOCEAN_UART_NAME       "enocean_uart"
#define ENOCEAN_UART_PREFIX     "[" ENOCEAN_UART_NAME "]"

struct enocean_uart {
    struct serdev_device *serdev;
    unsigned int baud;
};

struct enocean_uart *uart;


static int enocean_uart_receive_buf(struct serdev_device *serdev, const unsigned char *buf, size_t size)
{
    pr_info("%s", __func__);
    return 0;
}

static void enocean_uart_write_wake_up(struct serdev_device *serdev)
{
    pr_info("%s", __func__);
}

static const struct serdev_device_ops enocean_uart_device_ops = {
    .receive_buf = enocean_uart_receive_buf,
    .write_wakeup = enocean_uart_write_wake_up,
};

static int enocean_uart_probe(struct serdev_device *serdev)
{
    struct device *dev = &serdev->dev;
    int ret = 0;
    u32 baud;
    char buf[] = ENOCEAN_UART_PREFIX " hello world";

    uart = kzalloc(sizeof(struct enocean_uart), GFP_KERNEL);
    if (!uart) {
        dev_err(dev, "Failed to allocate struct enocean uart");
        ret = -ENOMEM;
        goto alloc_uart_err;
    }

    if (of_property_read_u32(dev->of_node, "current-speed", &baud)) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
		ret = -EINVAL;
        goto serdev_speed_err;
    }
    serdev_device_set_client_ops(serdev, &enocean_uart_device_ops);

    uart->baud = baud;
    uart->serdev = serdev;

    dev_info(dev, "compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    if ((ret = serdev_device_open(serdev)) < 0) {
        dev_err(dev, "Failed to open serial device\n");
        goto serdev_open_err;
    }

    if (serdev_device_set_baudrate(serdev, baud) != baud) {
        dev_err(dev, "Failed to set baudrate\n");
        ret = -EINVAL;
        goto set_baud_err; 
    }

    serdev_device_write(serdev, buf, sizeof(buf), 1000);

    serdev_device_close(serdev);

    dev_info(dev,"probed successfully\n");

    return ret;
    
    set_baud_err:
        serdev_device_close(serdev);
    serdev_open_err:
    serdev_speed_err:
        kfree(uart);   
    alloc_uart_err:
        return ret;
}

static const struct of_device_id enocean_uart_dt_ids[] = {
	{ .compatible = "enocean-uart" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, enocean_uart_dt_ids);

static struct serdev_device_driver enocean_uart_drv = {
    .probe = enocean_uart_probe,
    .driver = {
        .name       = "enocean_uart",
        .of_match_table = of_match_ptr(enocean_uart_dt_ids),
    },
};
module_serdev_device_driver(enocean_uart_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
MODULE_DESCRIPTION("En0cean uart driver");