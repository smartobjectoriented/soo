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

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <linux/serial_core.h>

#include <soo/dev/vvalve.h>
#include <soo/dev/venoceansw.h>
#include <soo/dev/vdogablind.h>
#include <soo/core/device_access.h>


extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);
extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern void uart_do_close(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);


typedef struct {

	/* Must be the first field */
	venoceansw_t venoceansw;
	venoceansw_ascii_data_t ascii_data;

} venoceansw_priv_t;



static struct vbus_device *venoceansw_dev = NULL;

void venoceansw_notify(struct vbus_device *vdev)
{
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&vdev->dev);

	venoceansw_ring_response_ready(&venoceansw_priv->venoceansw.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(venoceansw_priv->venoceansw.irq);
}


irqreturn_t venoceansw_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&vdev->dev);
	venoceansw_request_t *ring_req;
	// venoceansw_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = venoceansw_get_ring_request(&venoceansw_priv->venoceansw.ring)) != NULL) {

		// ring_rsp = venoceansw_new_ring_response(&venoceansw_priv->venoceansw.ring);

		// memcpy(ring_rsp->buffer, ring_req->buffer, VENOCEANSW_PACKET_SIZE);

		// venoceansw_ring_response_ready(&venoceansw_priv->venoceansw.ring);

		// notify_remote_via_virq(venoceansw_priv->venoceansw.irq);
	}

	return IRQ_HANDLED;
}

/**
 * Send Switch command & ID to frontend
 **/
void send_cmd_to_fe(int sw_cmd, int sw_id) {

	venoceansw_response_t *ring_rsp;
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&venoceansw_dev->dev);

	ring_rsp = venoceansw_new_ring_response(&venoceansw_priv->venoceansw.ring);

	ring_rsp->sw_id = sw_id;
	ring_rsp->sw_cmd = sw_cmd;

	venoceansw_ring_response_ready(&venoceansw_priv->venoceansw.ring);

	notify_remote_via_virq(venoceansw_priv->venoceansw.irq);

}

//Used to regroup the 4 ID bytes into one 32bits 
int32_t Switch_ID_Reconstitution(char ID[4])
{
	int32_t Full_ID = ((ID[0] << 24) | (ID[1] << 16) | (ID[2] << 8) | ID[3]);
	return Full_ID;
}


static int click_monitor_fn(void *args) {
	struct tty_struct *tty_uart;
	int len, nbytes;
	char buffer[VENOCEANSW_FRAME_SIZE];
	dev_t dev;
	int baud = 57600; //default baudrate for the enocean module
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	bool problem = false;
	int sw_id;

	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&venoceansw_dev->dev);

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
			while (nbytes < VENOCEANSW_INIT_SIZE) 
			{
				len = tty_do_read(tty_uart, buffer + nbytes, VENOCEANSW_INIT_SIZE);
				nbytes += len;
			}

			problem = false; //reset the problem state
		} 

		nbytes = 0; //reset the number of the actual byte to read

		//read the number of bytes corresponding to the size of the switch frame
		while (nbytes < VENOCEANSW_FRAME_SIZE) 
		{
			len = tty_do_read(tty_uart, buffer + nbytes, VENOCEANSW_FRAME_SIZE);
			nbytes += len;
		}

		/* Update the local asci data with the new received data. */
		memcpy(&venoceansw_priv->ascii_data, buffer, VENOCEANSW_FRAME_SIZE);


		//Test the differents possibilities of position
		if (venoceansw_priv->ascii_data.switch_data == SWITCH_IS_UP)
		{
			sw_id = Switch_ID_Reconstitution(venoceansw_priv->ascii_data.switch_ID);
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is up", sw_id);

			send_cmd_to_fe(SWITCH_IS_UP, sw_id);

// bypass FE to test
#if 0
			vdoga_motor_set_direction(1);
			vdoga_motor_set_percentage_speed(100);
			vdoga_motor_enable();
			
			vanalog_valve_open();
#endif
		} else if(venoceansw_priv->ascii_data.switch_data == SWITCH_IS_DOWN) {
			
			sw_id = Switch_ID_Reconstitution(venoceansw_priv->ascii_data.switch_ID);
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is down", sw_id);

			send_cmd_to_fe(SWITCH_IS_DOWN, sw_id);
// bypass FE to test
#if 0

			vdoga_motor_set_direction(0);
			vdoga_motor_set_percentage_speed(100);
			vdoga_motor_enable();

			vanalog_valve_close();
#endif


		} else if(venoceansw_priv->ascii_data.switch_data == SWITCH_IS_RELEASED) {

			sw_id = Switch_ID_Reconstitution(venoceansw_priv->ascii_data.switch_ID);
			//print the informations about the switch state
			printk("switch (ID: 0x%08X) is released", sw_id);

			send_cmd_to_fe(SWITCH_IS_RELEASED, sw_id);
// bypass FE to test
#if 0			
			vdoga_motor_disable();

#endif
		}  else {

			printk("invalid param");
			problem = true;
		} 
		printk("\n");
	}
	return 0;
}


void venoceansw_probe(struct vbus_device *vdev) {
	venoceansw_priv_t *venoceansw_priv;

	venoceansw_priv = kzalloc(sizeof(venoceansw_priv_t), GFP_ATOMIC);
	BUG_ON(!venoceansw_priv);

	dev_set_drvdata(&vdev->dev, venoceansw_priv);

	venoceansw_dev = vdev;

	printk("%s BACKEND PROBE CALLED", VENOCEANSW_PREFIX);

	kthread_run(click_monitor_fn, NULL, "click_enocean_switch_monitor");

	DBG(VENOCEANSW_PREFIX "Backend probe: %d\n", vdev->otherend_id);

}

void venoceansw_remove(struct vbus_device *vdev) {
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the venoceansw structure for %s\n", __func__,vdev->nodename);
	kfree(venoceansw_priv);
}


void venoceansw_close(struct vbus_device *vdev) {
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&vdev->dev);

	DBG(VENOCEANSW_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&venoceansw_priv->venoceansw.ring, (&venoceansw_priv->venoceansw.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(venoceansw_priv->venoceansw.irq, vdev);

	vbus_unmap_ring_vfree(vdev, venoceansw_priv->venoceansw.ring.sring);
	venoceansw_priv->venoceansw.ring.sring = NULL;
}

void venoceansw_suspend(struct vbus_device *vdev) {

	DBG(VENOCEANSW_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void venoceansw_resume(struct vbus_device *vdev) {

	DBG(VENOCEANSW_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void venoceansw_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	venoceansw_sring_t *sring;
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(&vdev->dev);

	DBG(VENOCEANSW_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&venoceansw_priv->venoceansw.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, venoceansw_interrupt, NULL, 0, VENOCEANSW_NAME "-backend", vdev);

	BUG_ON(res < 0);

	venoceansw_priv->venoceansw.irq = res;
}

void venoceansw_connected(struct vbus_device *vdev) {

	DBG(VENOCEANSW_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

#if 0
/*
 * Testing code to analyze the behaviour of the ME during pre-suspend operations.
 */
int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!venoceansw_start(i))
				continue;

			venoceansw_ring_response_ready()

			venoceansw_notify(i);

			venoceansw_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t venoceanswdrv = {
	.probe = venoceansw_probe,
	.remove = venoceansw_remove,
	.close = venoceansw_close,
	.connected = venoceansw_connected,
	.reconfigured = venoceansw_reconfigured,
	.resume = venoceansw_resume,
	.suspend = venoceansw_suspend
};

int venoceansw_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "venoceansw,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "venoceansw-gen");
#endif

	printk("VENOCEAN BACK INIT \n");
	devaccess_set_devcaps(DEVCAPS_CLASS_APP, DEVCAP_APP_DOMO, true);

	vdevback_init(VENOCEANSW_NAME, &venoceanswdrv);

	return 0;
}

device_initcall(venoceansw_init);
