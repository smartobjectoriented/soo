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

#include <asm/io.h>

void display_led(int led_nr, bool on);

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
		.compatible = "vsenseled,backend",
	},

	{ /* sentinel */ },
};

static struct platform_driver senseled_driver = {
	.probe = senseled_probe,
	.remove = senseled_remove,
	.driver = {
		.name = "vsenseled",
		.of_match_table = senseled_of_ids,
		.owner = THIS_MODULE,
	},
};

static int senseled_init(void) {

	printk("access: small driver for accessing Sense HAT led...\n");

	platform_driver_register(&senseled_driver);

	return 0;
}

static void senseled_exit(void) {

	platform_driver_unregister(&senseled_driver);
	printk("senseled: bye bye!\n");
}

module_init(senseled_init);
module_exit(senseled_exit);

MODULE_INFO(intree, "Y");
MODULE_LICENSE("GPL");

