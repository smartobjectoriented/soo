/*******************************************************************
 * access.c
 *
 * Copyright (c) 2020 HEIG-VD, REDS Institute
 *******************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <soo/vbus.h>

#include <asm/io.h>

typedef void(*joystick_handler_t)(struct vbus_device *vdev, int key);

#define UP      0x04
#define DOWN    0x01
#define RIGHT   0x02
#define LEFT    0x10
#define CENTER  0x08

void display_led(int led_nr, bool on);
void senseled_init(void);
void sensej_init(void);
void rpisense_joystick_handler_register(struct vbus_device *vdev, joystick_handler_t joystick_handler);

void j_handler(struct vbus_device *vdev, int key) {

	printk("%s: getting key %d\n", __func__, key);
}

static int senseled_probe(struct platform_device *pdev) {

	printk("%s: probing now...\n", __func__);

	display_led(3, true);

	return 0;

}

static int senseled_remove(struct platform_device *pdev) {

	printk("%s: releasing all...\n", __func__);

	return 0;
}

static const struct of_device_id senseled_of_ids[] = {
	{
		.compatible = "agency,rpi4",
	},

	{ /* sentinel */ },
};

static struct platform_driver senseled_driver = {
	.probe = senseled_probe,
	.remove = senseled_remove,
	.driver = {
		.name = "senseled",
		.of_match_table = senseled_of_ids,
		.owner = THIS_MODULE,
	},
};

static int mod_senseled_init(void) {
	struct vbus_device *cookie = (void *) 0xdead;

	printk("access: small driver for accessing Sense HAT led...\n");
	senseled_init();

	sensej_init();
	rpisense_joystick_handler_register(cookie, j_handler);

	platform_driver_register(&senseled_driver);

	return 0;
}

static void mod_senseled_exit(void) {

	platform_driver_unregister(&senseled_driver);
	printk("senseled: bye bye!\n");
}

module_init(mod_senseled_init);
module_exit(mod_senseled_exit);

MODULE_INFO(intree, "Y");
MODULE_LICENSE("GPL");

