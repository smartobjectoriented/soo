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
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <asm/io.h>

#include <soo/vbus.h>
#include <soo/evtchn.h>

#include <soo/dev/vvext.h>

#include "rpisense.h"

#define QEMU 0
#define VVEXT 1

char *vext_led_name[]  = {
	"vext_led0",
	"vext_led1",
	"vext_led2",
	"vext_led3",
	"vext_led4",
};

typedef struct {
	struct led_classdev led_cdev;
	int lednr;
	const char *name;
} vext_led_t;

typedef struct {
	struct input_dev *input;
	int switch_nr;
} switch_input_t;

static int keys[] = {KEY_ENTER, KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN};

typedef struct {
	void *vext_vaddr;

	/* Useful to put here for removal purpose */
	vext_led_t vext_led[5];

	switch_input_t switch_input;

	vvext_t vvext;

	/* True if vExpress, false if Rpi4 */
	bool vexpress;
} pdev_vext_t;

static irqreturn_t process_input(int irq, void *dev_id) {
	pdev_vext_t *pdev_vext = (pdev_vext_t *) dev_id;
	vvext_response_t *vvext_response;

#if VVEXT
	if (vdevfront_is_connected(pdev_vext->vvext.vdev)) {

		vdevback_processing_begin(pdev_vext->vvext.vdev);

		vvext_response = vvext_new_ring_response(&pdev_vext->vvext.ring);

		vvext_response->type = EV_KEY;
		vvext_response->code = keys[pdev_vext->switch_input.switch_nr-1];
		vvext_response->value = 1;

		vvext_ring_response_ready(&pdev_vext->vvext.ring);

		notify_remote_via_virq(pdev_vext->vvext.irq);

		vdevback_processing_end(pdev_vext->vvext.vdev);

	}
#else
	input_report_key(pdev_vext->switch_input.input, keys[pdev_vext->switch_input.switch_nr-1], 1);
	input_sync(pdev_vext->switch_input.input);
	input_report_key(pdev_vext->switch_input.input, keys[pdev_vext->switch_input.switch_nr-1], 0);
	input_sync(pdev_vext->switch_input.input);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t switch_isr(int irq, void *dev_id)
{
	pdev_vext_t *pdev_vext = (pdev_vext_t *) dev_id;
	int i;

	/* Ack */
	*((uint16_t *) (pdev_vext->vext_vaddr + 0x18)) = 0x81;

	/* Get the switch nr */
	pdev_vext->switch_input.switch_nr = *((uint16_t *) (pdev_vext->vext_vaddr + 0x12));

	for (i = 0; i < 7; i++)
		if (pdev_vext->switch_input.switch_nr & (1 << i))
			break;

	pdev_vext->switch_input.switch_nr = i+1;

	printk("## Switch nr. %d\n", pdev_vext->switch_input.switch_nr);

	return IRQ_WAKE_THREAD;
}

void led_set(struct led_classdev *led_cdev, enum led_brightness brightness) {
	vext_led_t *vext_led = container_of(led_cdev, vext_led_t, led_cdev);
	struct platform_device *pdev = container_of(led_cdev->dev->parent, struct platform_device, dev);
	pdev_vext_t *pdev_vext = (pdev_vext_t *) platform_get_drvdata(pdev);

	volatile uint16_t *led_vaddr = (uint16_t *) (pdev_vext->vext_vaddr + 0x3a);

#if 1
	printk("## led nr %d  brightness: %d\n", vext_led->lednr, brightness);
#endif

	switch (brightness) {
	case LED_ON:
		if (pdev_vext->vexpress)
			*led_vaddr |= 1 << vext_led->lednr;
		else
			display_led(vext_led->lednr, true);

		break;

	case LED_OFF:
		if (pdev_vext->vexpress)
			*led_vaddr &= ~(1 << vext_led->lednr);
		else
			display_led(vext_led->lednr, false);

		break;

	default:
		break;
	}
}

void rpisense_process_switch( struct platform_device *pdev, int key) {
	pdev_vext_t *pdev_vext = (pdev_vext_t *) platform_get_drvdata(pdev);
	uint32_t key_input;
	vvext_response_t *vvext_response;

	printk("### %s: got %d\n", __func__, key);

	switch (key){
		case 1:
			key_input = KEY_ENTER;
			break;
		case 2:
			key_input = KEY_LEFT;
			break;
		case 3:
			key_input = KEY_UP;
			break;
		case 4:
			key_input = KEY_RIGHT;
			break;
		case 5:
			key_input = KEY_DOWN;
			break;
		default:
			return;
			break;
	}

#if VVEXT
	if (vdevfront_is_connected(pdev_vext->vvext.vdev)) {

		vdevback_processing_begin(pdev_vext->vvext.vdev);

		vvext_response = vvext_new_ring_response(&pdev_vext->vvext.ring);

		vvext_response->type = EV_KEY;
		vvext_response->code = keys[pdev_vext->switch_input.switch_nr-1];
		vvext_response->value = 1;

		vvext_ring_response_ready(&pdev_vext->vvext.ring);

		notify_remote_via_virq(pdev_vext->vvext.irq);

		vdevback_processing_end(pdev_vext->vvext.vdev);

	}
#else
	input_report_key(pdev_vext->switch_input.input, keys[pdev_vext->switch_input.switch_nr-1], 1);
	input_sync(pdev_vext->switch_input.input);
	input_report_key(pdev_vext->switch_input.input, keys[pdev_vext->switch_input.switch_nr-1], 0);
	input_sync(pdev_vext->switch_input.input);
#endif
}


irqreturn_t vvext_interrupt(int irq, void *dev_id)
{
	vvext_request_t *ring_req;
	int led;
	pdev_vext_t *pdev_vext;
	vvext_t *vvext = (vvext_t *) dev_id;

	pdev_vext = container_of(dev_id, pdev_vext_t, vvext);

	while ((ring_req = vvext_get_ring_request(&vvext->ring)) != NULL) {

		led = ring_req->buffer[0];

		led_set(&pdev_vext->vext_led[led].led_cdev, ((ring_req->buffer[1] == '1') ? LED_ON : LED_OFF));
	}

	return IRQ_HANDLED;
}

static int vext_probe_common(struct platform_device *pdev, pdev_vext_t *pdev_vext) {
	int i;
	int ret;

	/* Register our leds to the LED framework */
	for (i = 0; i < 5; i++) {
		pdev_vext->vext_led[i].lednr = i;
		pdev_vext->vext_led[i].led_cdev.name = vext_led_name[i];
		pdev_vext->vext_led[i].led_cdev.brightness_set = led_set;

		ret = led_classdev_register(&pdev->dev, &pdev_vext->vext_led[i].led_cdev);
		if (ret < 0) {
			printk("led_classdev_register failed!\n");
			return -1;
		}
	}

	pdev_vext->switch_input.input = input_allocate_device();

	if (!pdev_vext->switch_input.input) {
		printk("failed to allocate input device...\n");
		return -1;
	}
	pdev_vext->switch_input.input->name = "vext_switch";
	pdev_vext->switch_input.input->dev.parent = &pdev->dev;

	for (i = 0; i < 5; i++)
		input_set_capability(pdev_vext->switch_input.input, EV_KEY, keys[i]);


	ret = input_register_device(pdev_vext->switch_input.input);
	if (ret) {
		printk("input register failed (%d)...\n", ret);
		return -1;
	}
#if VVEXT
	vvext_init(&pdev_vext->vvext, vvext_interrupt);
#endif
	return 0;
}


static int vext_probe(struct platform_device *pdev) {
	int irq;
	int ret;
	struct resource *iores;
	pdev_vext_t *pdev_vext;

	printk("## Now probing vext...\n");

	pdev_vext = kzalloc(sizeof(pdev_vext_t), GFP_KERNEL);
        BUG_ON(!pdev_vext);

        platform_set_drvdata(pdev, pdev_vext);

        pdev_vext->vexpress = true;

        iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        printk("## base: %x size: %d\n", iores->start, iores->end-iores->start+1);

        pdev_vext->vext_vaddr = ioremap(iores->start, iores->end-iores->start+1);

#if 0
        /* LEDS on */
        *((uint16_t *) (pdev_vext->vext_vaddr + 0x3a)) = 0x2a;
#endif

        /* Enable IRQ */
        *((uint16_t *) (pdev_vext->vext_vaddr + 0x18)) = 0x80;

        irq = platform_get_irq(pdev, 0);

        ret = request_threaded_irq(irq, switch_isr, process_input, IRQF_TRIGGER_HIGH, "vext_switch", pdev_vext);

        return vext_probe_common(pdev, pdev_vext);

}

static int vext_rpisense_probe(struct platform_device *pdev) {
	pdev_vext_t *pdev_vext;

	printk("## Now probing rpisense vext...\n");

	pdev_vext = kzalloc(sizeof(pdev_vext_t), GFP_KERNEL);
        BUG_ON(!pdev_vext);

	platform_set_drvdata(pdev, pdev_vext);

	pdev_vext->vexpress = false;

	rpisense_init();
	rpisense_joystick_handler_register(pdev, rpisense_process_switch);

	return vext_probe_common(pdev, pdev_vext);

}


static int vext_remove(struct platform_device *pdev) {
	int irq;
	int i;
	pdev_vext_t *pdev_vext;

	printk("%s: releasing all...\n", __func__);

	pdev_vext = (pdev_vext_t *) platform_get_drvdata(pdev);

	irq = platform_get_irq(pdev, 0);
	free_irq(irq, pdev_vext);

	for (i = 0; i < 5; i++)
		led_classdev_unregister(&pdev_vext->vext_led[i].led_cdev);

	iounmap(pdev_vext->vext_vaddr);

	input_unregister_device(pdev_vext->switch_input.input);

	kfree(pdev_vext);

	return 0;
}

static const struct of_device_id vext_of_ids[] = {
	{
		.compatible = "vexpress,vext",
	},
	{ /* sentinel */ },
};

static struct platform_driver vext_driver = {
	.probe = vext_probe,
	.remove = vext_remove,
	.driver = {
		.name = "vext",
		.of_match_table = vext_of_ids,
		.owner = THIS_MODULE,
	},
};

static const struct of_device_id vext_rpisense_of_ids[] = {
	{
		.compatible = "rpi4,vext",
	},
	{ /* sentinel */ },
};

static struct platform_driver vext_rpisense_driver = {
	.probe = vext_rpisense_probe,
	.remove = vext_remove,
	.driver = {
		.name = "vext",
		.of_match_table = vext_rpisense_of_ids,
		.owner = THIS_MODULE,
	},
};

static int access_init(void) {

	printk("access: small driver for accessing I/O ...\n");

#if QEMU
	platform_driver_register(&vext_driver);
#else
	platform_driver_register(&vext_rpisense_driver);
#endif

	return 0;
}

static void access_exit(void) {
	rpisense_exit();

	platform_driver_unregister(&vext_driver);
	printk("access: bye bye!\n");
}

module_init(access_init);
module_exit(access_exit);

MODULE_INFO(intree, "Y");
MODULE_LICENSE("GPL");
