/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016 Baptiste Delporte <bonel@bonel.net>
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
#define DEBUG
#endif

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <asm/termios.h>
#include <linux/serial_core.h>
#include <soo/dev/venocean.h>
#include <soo/dev/vdoga.h>
#include <soo/dev/vanalog.h>

extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);
extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern void uart_do_close(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);

int32_t Switch_ID_Reconstitution(char ID[4]);

/* ASCII data coming from the enocean module*/
static venocean_ascii_data_t ascii_data;

static struct gpio_desc *SW1_gpio;
static struct gpio_desc *SW2_gpio;

int enOcean_mode = ENOCEAN_MODE_BLIND;

static int click_monitor_fn(void *args) {
	struct tty_struct *tty_uart;
	int len, nbytes;
	char buffer[VENOCEAN_FRAME_SIZE];
	dev_t dev;
	int baud = 57600; //default baudrate for the enocean module
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int i = 0;
	bool problem = false;

	lprintk("%s: starting to acquire switch data from the enocean module.\n", __func__);

	/* Initiate the tty device dedicated to the enocean module. */
	tty_dev_name_to_number(ENOCEAN_UART5_DEV, &dev);
	tty_uart = tty_kopen(dev);

	uart_do_open(tty_uart);

	/* Set the termios parameters related to tty. */

	tty_uart->termios.c_lflag = NOFLSH;
	tty_set_termios(tty_uart, &tty_uart->termios);

	/* Set UART configuration */
	uart_set_options(((struct uart_state *) tty_uart->driver_data)->uart_port, NULL, baud, parity, bits, flow);



	while (true) 
	{
		nbytes = 0; //reset the number of the actual byte to read

		//if a problem was detected during the precedent iteration
		if(problem == true)
		{
			//read the number of byte corresponding to the enocean reset
			while (nbytes < VENOCEAN_INIT_SIZE) 
			{
				len = tty_do_read(tty_uart, buffer + nbytes, VENOCEAN_INIT_SIZE);
				nbytes += len;
			}

			problem = false; //reset the problem state
		} 

		nbytes = 0; //reset the number of the actual byte to read

		//read the number of bytes corresponding to the size of the switch frame
		while (nbytes < VENOCEAN_FRAME_SIZE) 
		{
			len = tty_do_read(tty_uart, buffer + nbytes, VENOCEAN_FRAME_SIZE);
			nbytes += len;
		}

		/* Update the local asci data with the new received data. */
		memcpy(&ascii_data, buffer, VENOCEAN_FRAME_SIZE);


		//Test the differents possibilities of position
		if (ascii_data.switch_data == SWITCH_IS_UP)
		{
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is up", Switch_ID_Reconstitution(ascii_data.switch_ID));

			//test the actual mode (blind or valve)
			if(enOcean_mode == ENOCEAN_MODE_BLIND)
			{
				vdoga_blind_up();
			}
			else
			{
				vanalog_valve_open();
			}
		}
		else if(ascii_data.switch_data == SWITCH_IS_DOWN)
		{
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is down", Switch_ID_Reconstitution(ascii_data.switch_ID));

			//test the actual mode (blind or valve)
			if(enOcean_mode == ENOCEAN_MODE_BLIND)
			{
				vdoga_blind_down();
			}
			else
			{
				vanalog_valve_close();
			}
		}
		else if(ascii_data.switch_data == SWITCH_IS_RELEASED)
		{
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is released", Switch_ID_Reconstitution(ascii_data.switch_ID));

			//test the actual mode (blind or valve)
			if(enOcean_mode == ENOCEAN_MODE_BLIND)
			{
				vdoga_blind_stop();
			}
		} 
		else
		{
			printk("invalid param");
			problem = true;
		} 
		printk("\n");
	}
	return 0;
}

static int switch_between_modes(void *args) 
{
	int SW1_save[4] = {1, 1, 1, 1}; //array to save the 4 last values from SW1

	while(1)
	{
		msleep(GPIO_POLL_PERIOD); 

		//shift the values inside the array and get a new one
		SW1_save[3] = SW1_save[2];	
		SW1_save[2] = SW1_save[1];
		SW1_save[1] = SW1_save[0];
		SW1_save[0]	= gpiod_get_value(SW1_gpio);

		//Test the 4 last values of SW1 to detect a failing edge
		if((SW1_save[3] == 1) && (SW1_save[2] == 1) 
			&& (SW1_save[1] == 0) && (SW1_save[0] == 0))
		{
			//test the actual state
			if(enOcean_mode == ENOCEAN_MODE_BLIND)
			{
				enOcean_mode = ENOCEAN_MODE_VALVE;
				printk("enOcean switch will now drive the valve");
			}
			else
			{
				enOcean_mode = ENOCEAN_MODE_BLIND;	
				printk("enOcean switch will now drive the blind");
			}
			printk("\n");
		}
		
	}
	return 0;
}

//Used to regroup the 4 ID bytes into one 32bits 
int32_t Switch_ID_Reconstitution(char ID[4])
{
	int32_t Full_ID = ((ID[0] << 24) | (ID[1] << 16) | (ID[2] << 8) | ID[3]);
	return Full_ID;
}

static void process_response(struct vbus_device *vdev) {
	venocean_t *venocean = to_venocean(vdev);
	venocean_request_t *ring_req;
	venocean_response_t *ring_rsp;
	struct winsize wsz;

	ring_rsp = venocean_ring_response(&venocean->ring);
	venocean_ring_response_ready(&venocean->ring);

	notify_remote_via_virq(venocean->irq);
}

irqreturn_t venocean_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	return IRQ_HANDLED;
}


//setup the gpios used in venocean backend
static void setup_gpios(struct device *dev) {
	int ret;

	//gpio used as input to get the value from SW2
	SW2_gpio = gpiod_get(dev, "SW2", GPIOD_OUT_HIGH);
	printk("SW2_gpio = 0x%08X\n", SW2_gpio);
	if (IS_ERR(SW2_gpio)) {
		ret = PTR_ERR(SW2_gpio);
		dev_err(dev, "Failed to get SW2 GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(SW2_gpio); 

	//gpio used as input to get the value from SW1
	SW1_gpio = gpiod_get(dev, "SW1", GPIOD_OUT_HIGH);
	printk("SW1_gpio = 0x%08X\n", SW1_gpio);
	if (IS_ERR(SW1_gpio)) {
		ret = PTR_ERR(SW1_gpio);
		dev_err(dev, "Failed to get SW1 GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(SW1_gpio); 
}

void venocean_probe(struct vbus_device *vdev) 
{
	venocean_t *venocean;

	venocean = kzalloc(sizeof(venocean_t), GFP_ATOMIC);
	BUG_ON(!venocean);

	spin_lock_init(&venocean->ring_lock);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "venocean,backend");

	dev_set_drvdata(&vdev->dev, &venocean->vdevback);

	setup_gpios(&vdev->dev);

	kthread_run(click_monitor_fn, NULL, "click_enocean_monitor");

	kthread_run(switch_between_modes, NULL, "switch_between_mode");

	DBG(VENOCEAN_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void venocean_remove(struct vbus_device *vdev) {
	venocean_t *venocean = to_venocean(vdev);


	DBG("%s: freeing the venocean structure for %s\n", __func__,vdev->nodename);
	kfree(venocean);
}

void venocean_close(struct vbus_device *vdev) {
	venocean_t *venocean = to_venocean(vdev);

	DBG("(venocean) Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&venocean->ring, (&venocean->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(venocean->irq, vdev);

	vbus_unmap_ring_vfree(vdev, venocean->ring.sring);
	venocean->ring.sring = NULL;
}

void venocean_suspend(struct vbus_device *vdev) {

	DBG("(venocean) Backend suspend: %d\n", vdev->otherend_id);
}

void venocean_resume(struct vbus_device *vdev) {

	DBG("(venocean) Backend resume: %d\n", vdev->otherend_id);
}

void venocean_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	venocean_sring_t *sring;
	venocean_t *venocean = to_venocean(vdev);

	DBG(VENOCEAN_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&venocean->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, venocean_interrupt, NULL, 0, VENOCEAN_NAME "-backend", vdev);

	BUG_ON(res < 0);

	venocean->irq = res;
}

void venocean_connected(struct vbus_device *vdev) {

	DBG(VENOCEAN_PREFIX "Backend connected: %d\n",vdev->otherend_id);

	// process_response(vdev);
}


vdrvback_t venoceandrv = {
	.probe = venocean_probe,
	.remove = venocean_remove,
	.close = venocean_close,
	.connected = venocean_connected,
	.reconfigured = venocean_reconfigured,
	.resume = venocean_resume,
	.suspend = venocean_suspend
};

int venocean_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "venocean,backend");

	/* Check if DTS has venocean enabled */
	if (!of_device_is_available(np))
		return 0;

	// INIT_LIST_HEAD(&vdev_consoles);

	vdevback_init(VENOCEAN_NAME, &venoceandrv);

	printk("VENOCEAN INITILIZED\n");

	return 0;
}

device_initcall(venocean_init);
