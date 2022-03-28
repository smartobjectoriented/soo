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

#define ENOCEAN_UART_NAME       "enocean_uart"
#define ENOCEAN_UART_PREFIX     "[" ENOCEAN_UART_NAME "]"


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

static const struct serdev_controller_ops controller_ops = {
    .set_baudrate = NULL,
};
static int enocean_uart_probe(struct serdev_device *serdev)
{
    struct device *dev = &serdev->dev;
    struct tty_struct *uart;
    u32 baud;

    if (of_property_read_u32(dev->of_node, "current-speed", &baud)) {
        dev_err(dev, "'current-speed' is not specified in device node\n");
		return -EINVAL;
    }

    pr_info(ENOCEAN_UART_PREFIX " node name: %s, driver name: %s", dev->of_node->parent->name, 
                    dev->of_node->parent->parent->name);

    // uart = tty_kopen(dev->devt);
    // if (!uart) {
    //     dev_err(dev, "failed to get tty_struct\n");
    //     return -1;
    // }

    pr_info(ENOCEAN_UART_PREFIX " drv name: %s", uart->name);

    serdev_device_set_client_ops(serdev, &enocean_uart_device_ops);

    pr_info(ENOCEAN_UART_PREFIX " compatible %s, baud: %d", dev->driver->of_match_table->compatible, baud);

    // serdev->ctrl->ops = &controller_ops;

    // BLOCKING_INIT_NOTIFIER_HEAD(&event_notifier_list);
    // serdev_device_open(serdev);
    // if (devm_serdev_device_open(dev, serdev)) {
    //     dev_err(dev, "Failed to open enocean uart");
    //     return -1;
    // }
    // serdev_device_set_baudrate(serdev, baud);

    pr_info(ENOCEAN_UART_PREFIX " probed\n");

    return 0;
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

// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("Mattia Gallacchi <mattia.gallacchi@heig-vd.ch");
// MODULE_DESCRIPTION("En0cean uart driver");