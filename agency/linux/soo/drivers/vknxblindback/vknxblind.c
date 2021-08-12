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

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

#include <soo/dev/vknxblind.h>

#include <asm/termios.h>
#include <linux/serial_core.h>
#include <linux/tty.h>


extern struct tty_struct *tty_kopen(dev_t device);
extern void uart_do_open(struct tty_struct *tty);
extern void uart_do_close(struct tty_struct *tty);
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios);
extern int uart_do_write(struct tty_struct *tty, const unsigned char *buf, int count);
extern void n_tty_do_flush_buffer(struct tty_struct *tty);
extern ssize_t tty_do_read(struct tty_struct *tty, unsigned char *buf, size_t nr);

typedef struct {

	/* Must be the first field */
	vknxblind_t vknxblind;

	struct tty_struct *tty_uart;
	struct completion wait_cmd;
	int cmd_blind;

} vknxblind_priv_t;

static struct vbus_device *vknxblind_dev = NULL;

void vknxblind_notify(struct vbus_device *vdev)
{
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vdev->dev);

	vknxblind_ring_response_ready(&vknxblind_priv->vknxblind.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vknxblind_priv->vknxblind.irq);
}


irqreturn_t vknxblind_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vdev->dev);
	vknxblind_request_t *ring_req;
	vknxblind_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vknxblind_get_ring_request(&vknxblind_priv->vknxblind.ring)) != NULL) {

		ring_rsp = vknxblind_new_ring_response(&vknxblind_priv->vknxblind.ring);

		vknxblind_priv->cmd_blind = ring_req->knxblind_cmd;

		printk(VKNXBLIND_PREFIX "BE receive %d from FE\n", vknxblind_priv->cmd_blind);

		complete(&vknxblind_priv->wait_cmd);

		// memcpy(ring_rsp->buffer, ring_req->buffer, VKNXBLIND_PACKET_SIZE);

		// vknxblind_ring_response_ready(&vknxblind_priv->vknxblind.ring);

		// notify_remote_via_virq(vknxblind_priv->vknxblind.irq);
	}

	return IRQ_HANDLED;
}

void vknxblind_tty_flush(vknxblind_priv_t *vknxblind_priv) {

	// char rx_buffer[256];
	// int len;

	// while(1){
	// len = tty_do_read(vknxblind_priv->tty_uart, rx_buffer, 256);
	// printk(VKNXBLIND_PREFIX "flushed, %d bytes\n", len);
		// if(!(len > 0)) {
		// 	break;
		// }
	// }

	struct tty_struct *tty = vknxblind_priv->tty_uart;
	tty_buffer_flush(tty, NULL);
	tty_ldisc_flush(tty);
	// tty_driver_flush_buffer(tty);
	printk(VKNXBLIND_PREFIX "Flush eneded\n");

}
/**
 * \brief write byte by byte in tty 
 **/
int vknxblind_tty_write_buf(vknxblind_priv_t *vknxblind_priv, const unsigned char *data, size_t len)
{
	int i;
	int ret = 0;
	struct tty_struct *tty = vknxblind_priv->tty_uart;
	
	printk(VKNXBLIND_PREFIX "tx[%d]", len);
	for(i = 0; i < len; i++) {
		printk("%02hhx ", data[i]);
	}
	printk("\n");

	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	for(i = 0; i < len; i++) {

		ret += tty->ops->write(tty, &data[i], 1);
		udelay(1000);
	}

	return ret;
}

void vknxblind_send_ack(void) {
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vknxblind_dev->dev);

	char ack[1] = {0xe5};
	printk(VKNXBLIND_PREFIX "send ACK\n");
	vknxblind_tty_write_buf(vknxblind_priv, ack, 1);
}


void vknxblind_stop_blind(void){
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vknxblind_dev->dev);

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};
	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};
	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};
	char stop_blind_cmd[18] = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x02, 0x01, 0x00, 0x80, 0xAC, 0x16};
	unsigned char rx_buffer[256];
	int step = 0;
	int i;
	int rx_length;
	

	printk(VKNXBLIND_PREFIX "__________________________________________BLIND STOP !\n");

	// flush uart
	vknxblind_tty_flush(vknxblind_priv);
	// rx_length = tty_do_read(vknxblind_priv->tty_uart, (void*)rx_buffer, 255);
	// memset(rx_buffer, 0, 256);
	
	
	uart_do_write(vknxblind_priv->tty_uart, prop_read_req, 14);
	// uart_tx_string(prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = 0;

		while(rx_length < 256) {

			rx_length += tty_do_read(vknxblind_priv->tty_uart, rx_buffer + rx_length, 1);
			
			// printk("_______________READ");

			// for(i = 0; i < rx_length; i++) {

			// 	printk("%02hhx ", rx_buffer[i]);
			// }
			// printk("\n");

			// wait 			ACK or 					  data packet finishing with 0x16
			if(rx_buffer[0] == 0xe5 || (rx_length > 4 && rx_buffer[rx_length - 1] == 0x16)) {

				break;
			}
		}
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printk(VKNXBLIND_PREFIX "rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printk("%02hhx ", rx_buffer[i]);
			}
			printk("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				vknxblind_send_ack();
				
				if(step == 0) {
					
					uart_do_write(vknxblind_priv->tty_uart, prop_write_req, 15);
					// uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					uart_do_write(vknxblind_priv->tty_uart, prop_read_req4, 15);
					// uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printk(VKNXBLIND_PREFIX "___________________STOP BLIND\n");
					uart_do_write(vknxblind_priv->tty_uart, stop_blind_cmd, 18);
					// uart_tx_string(up_blind_cmd, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}

		}
	}
}

void vknxblind_up_blind(void){

	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vknxblind_dev->dev);

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};
	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};
	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};
	char up_blind_cmd[18]   = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x80, 0xAB, 0x16};
	unsigned char rx_buffer[256];
	int step = 0;
	int i;
	int rx_length;
	
	printk(VKNXBLIND_PREFIX "__________________________________________BLIND UP !\n");
	// flush uart
	vknxblind_tty_flush(vknxblind_priv);

	// rx_length = tty_do_read(vknxblind_priv->tty_uart, (void*)rx_buffer, 255);
	// memset(rx_buffer, 0, 256);
	
	
	uart_do_write(vknxblind_priv->tty_uart, prop_read_req, 14);
	// uart_tx_string(prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = 0;

		while(rx_length < 256) {

			rx_length += tty_do_read(vknxblind_priv->tty_uart, rx_buffer + rx_length, 1);
			
			// printk("_______________READ");

			// for(i = 0; i < rx_length; i++) {

			// 	printk("%02hhx ", rx_buffer[i]);
			// }
			// printk("\n");

			// wait 			ACK or 					  data packet finishing with 0x16
			if(rx_buffer[0] == 0xe5 || (rx_length > 4 && rx_buffer[rx_length - 1] == 0x16)) {

				break;
			}
		}
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printk(VKNXBLIND_PREFIX "rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printk("%02hhx ", rx_buffer[i]);
			}
			printk("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				vknxblind_send_ack();
				
				if(step == 0) {
					
					uart_do_write(vknxblind_priv->tty_uart, prop_write_req, 15);
					// uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					uart_do_write(vknxblind_priv->tty_uart, prop_read_req4, 15);
					// uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printk(VKNXBLIND_PREFIX "___________________UP BLIND\n");
					uart_do_write(vknxblind_priv->tty_uart, up_blind_cmd, 18);
					// uart_tx_string(up_blind_cmd, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}

		}
	}
}

void vknxblind_down_blind(void){
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vknxblind_dev->dev);

	const unsigned char prop_read_req[14]  = 
		{0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};

	const unsigned char prop_write_req[15] = 
		{0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};

	const unsigned char prop_read_req4[14] = 
		{0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};

	const unsigned char down_blind_cmd[18] = 
		{0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x81, 0xAC, 0x16};

	unsigned char rx_buffer[256];
	int step = 0;
	int i;
	int rx_length;


	printk(VKNXBLIND_PREFIX "__________________________________________BLIND DOWN !\n");
	// return;

	// printk("================ c_iflag %u\n", vknxblind_priv->tty_uart->termios.c_iflag);
	// printk("================ c_oflag %u\n", vknxblind_priv->tty_uart->termios.c_oflag);
	// printk("================ c_cflag %u\n", vknxblind_priv->tty_uart->termios.c_cflag);
	// printk("================ c_lflag %u\n", vknxblind_priv->tty_uart->termios.c_lflag);
	// printk("================ c_ispeed %u\n", vknxblind_priv->tty_uart->termios.c_ispeed);
	// printk("================ c_ospeed %u\n", vknxblind_priv->tty_uart->termios.c_ospeed);
	// printk("================ c_line %02hhx\n", vknxblind_priv->tty_uart->termios.c_line);

	printk("== c_cc :");
	for(i = 0; i < NCCS; i++) {
		printk("%02hhx", vknxblind_priv->tty_uart->termios.c_cc[i]);
	}
	
	printk(VKNXBLIND_PREFIX "________________________BE Store going DOWN !\n");

	// flush uart
	vknxblind_tty_flush(vknxblind_priv);
	
	vknxblind_tty_write_buf(vknxblind_priv, prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = 0;

		while(rx_length < 256) {

			rx_length += tty_do_read(vknxblind_priv->tty_uart, rx_buffer + rx_length, 1);
			
			// printk("_______________READ");

			// for(i = 0; i < rx_length; i++) {

			// 	printk("%02hhx ", rx_buffer[i]);
			// }
			// printk("\n");

			// wait 			ACK or 					  data packet finishing with 0x16
			if(rx_buffer[0] == 0xe5 || (rx_length > 4 && rx_buffer[rx_length - 1] == 0x16)) {

				break;
			}
		}
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printk(VKNXBLIND_PREFIX "rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printk("%02hhx ", rx_buffer[i]);
			}
			printk("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				vknxblind_send_ack();
				
				if(step == 0) {
					
					vknxblind_tty_write_buf(vknxblind_priv, prop_write_req, 15);
					// uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					vknxblind_tty_write_buf(vknxblind_priv, prop_read_req4, 15);
					// uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printk(VKNXBLIND_PREFIX "___________________DOWN BLIND\n");
					vknxblind_tty_write_buf(vknxblind_priv, down_blind_cmd, 18);
					// uart_tx_string(up_blind_cmd, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}
		}
	}
}

static int knxblind_monitor_fn(void *args){

	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vknxblind_dev->dev);


	printk(VKNXBLIND_PREFIX "BE receive %d\n", vknxblind_priv->cmd_blind);

	while(1) {

		wait_for_completion(&vknxblind_priv->wait_cmd);

		switch (vknxblind_priv->cmd_blind)
		{
		case VKNXBLIND_STOP_CMD:
			vknxblind_stop_blind();
			break;
		case VKNXBLIND_UP_CMD:
			vknxblind_up_blind();
			break;
		case VKNXBLIND_DOWN_CMD:
			vknxblind_down_blind();
			break;
		
		default:
			printk(VKNXBLIND_PREFIX " unknown command from FE\n");
			break;
		}
	}

	return 0;
}


unsigned int vknxblind_tty_set_baudrate(struct tty_struct *tty, unsigned int speed)
{
	struct ktermios ktermios = tty->termios;

	ktermios.c_cflag &= ~CBAUD;
	tty_termios_encode_baud_rate(&ktermios, speed, speed);

	/* tty_set_termios() return not checked as it is always 0 */
	tty_set_termios(tty, &ktermios);
	return ktermios.c_ospeed;
}

int vknxblind_tty_set_parity(struct tty_struct *_tty,
			      const char parity) {

	struct tty_struct *tty = _tty;

	struct ktermios ktermios = tty->termios;

	ktermios.c_cflag &= ~(PARENB | PARODD | CMSPAR);
	if (parity != 'n') {
		ktermios.c_cflag |= PARENB;
		if (parity == 'o')
			ktermios.c_cflag |= PARODD;
	}

	tty_set_termios(tty, &ktermios);
	printk("================tty = %u\n", (tty->termios.c_cflag & (PARENB | PARODD | CMSPAR)));
	printk("================kte = %u\n", (ktermios.c_cflag & (PARENB | PARODD | CMSPAR)));

	if ((tty->termios.c_cflag & (PARENB | PARODD | CMSPAR)) !=
	    (ktermios.c_cflag & (PARENB | PARODD | CMSPAR)))
		return -EINVAL;

	return 0;
}



void vknxblind_init_uart(vknxblind_priv_t *vknxblind_priv, const char* tty_port) {

	dev_t dev;
	int baud = 19200;
	int bits = 8;
	int parity = 'e';
	int flow = 'n';
	int ret = 0;
	unsigned int real_speed;
	struct ktermios ktermios;



    tty_dev_name_to_number(tty_port, &dev);

	vknxblind_priv->tty_uart = tty_kopen(dev);
	

	uart_do_open(vknxblind_priv->tty_uart);


	tty_init_termios(vknxblind_priv->tty_uart);

    printk(VKNXBLIND_PREFIX "UART opened \n");

	real_speed = vknxblind_tty_set_baudrate(vknxblind_priv->tty_uart, baud);


	printk(VKNXBLIND_PREFIX "tty_set_termios....\n");

	ktermios = vknxblind_priv->tty_uart->termios;
	/* Set the termios parameters related to the KNX kBerry 838 module. */
	ktermios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	ktermios.c_oflag &= ~(OPOST);
	ktermios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	ktermios.c_cflag &= ~(CSTOPB | CSIZE | CRTSCTS | PARODD | CMSPAR);
	ktermios.c_cflag |= (CS8 | CREAD | PARENB);


	tty_set_termios(vknxblind_priv->tty_uart, &ktermios);

	printk(VKNXBLIND_PREFIX "TTY options settled !\n");
	uart_set_options(((struct uart_state *) vknxblind_priv->tty_uart->driver_data)->uart_port, NULL, baud, parity, bits, flow);


	// ret = vknxblind_tty_set_parity(vknxblind_priv->tty_uart, 'e');
	
	// if(ret != 0){
	// 	printk("__________ERR PARITY SET....\n");
	// }

	// uart_do_close(vknxblind_priv->tty_uart);
	// tty_kclose(vknxblind_priv->tty_uart);

}

void vknxblind_reset_kBerry(vknxblind_priv_t *vknxblind_priv) {

	unsigned char rx_buffer[256];
	int rx_length, i, ret;
	bool pui_req_sended = false;
	// struct tty_struct *tty = vknxblind_priv->tty_uart;
	
	const unsigned char reset_tx[4]   = {0x10, 0x40, 0x40, 0x16};
	const unsigned char pei_req_tx[8] = {0x68, 0x02, 0x02, 0x68, 0x73, 0xA7, 0x1A, 0x16};


	ret = vknxblind_tty_write_buf(vknxblind_priv, reset_tx, 4);

	printk("======= NB WRITED %d\n", ret);

	printk(VKNXBLIND_PREFIX "Reset kBerry module !!\n");

	// printk("================ c_iflag %u\n", vknxblind_priv->tty_uart->termios.c_iflag);
	// printk("================ c_oflag %u\n", vknxblind_priv->tty_uart->termios.c_oflag);
	// printk("================ c_cflag %u\n", vknxblind_priv->tty_uart->termios.c_cflag);
	// printk("================ c_lflag %u\n", vknxblind_priv->tty_uart->termios.c_lflag);
	// printk("================ c_ispeed %u\n", vknxblind_priv->tty_uart->termios.c_ispeed);
	// printk("================ c_ospeed %u\n", vknxblind_priv->tty_uart->termios.c_ospeed);

	while(1){

		memset(rx_buffer, 0, 256);
		rx_length = 0;

		while(rx_length < 256) {

			rx_length += tty_do_read(vknxblind_priv->tty_uart, rx_buffer + rx_length, 1);
			
			// printk("_______________READ");

			// for(i = 0; i < rx_length; i++) {

			// 	printk("%02hhx ", rx_buffer[i]);
			// }
			// printk("\n");

			// wait 			ACK or 			data packet finishing with 0x16
			if(rx_buffer[0] == 0xe5 || (rx_length > 4 && rx_buffer[rx_length - 1] == 0x16)) {

				break;
			}
		}

		
		if(rx_length > 0) {

			printk(VKNXBLIND_PREFIX "rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printk("%02hhx ", rx_buffer[i]);
			}
			printk("\n");
			
			if(rx_buffer[0] == 0xe5) {

				if(!pui_req_sended) {

					vknxblind_tty_write_buf(vknxblind_priv, pei_req_tx, 8);
					pui_req_sended = true;
				}

			} else if(rx_buffer[rx_length - 1] == 0x16) {

				vknxblind_send_ack();
				break;
			}
		}
	}
}

void vknxblind_probe(struct vbus_device *vdev) {
	vknxblind_priv_t *vknxblind_priv;
	struct device_node *np = vdev->dev.of_node;
	const char *port_name = "ttyAMA3";
	int ret;

	vknxblind_priv = kzalloc(sizeof(vknxblind_priv_t), GFP_ATOMIC);
	BUG_ON(!vknxblind_priv);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "vknxblind,backend");

	dev_set_drvdata(&vdev->dev, vknxblind_priv);

	vknxblind_dev = vdev;

	/* var completion waiting for cmd from FE */
	init_completion(&vknxblind_priv->wait_cmd);

	/* GET TTY PORT FROM DTB */
	ret = of_property_read_string(np, "tty_port", &port_name);

	if(ret == -EINVAL) {
		printk("%s property tty_port doesn't exist !\n", VKNXBLIND_PREFIX);

	} else if (ret == -ENODATA) {
		printk("%s property port is null !\n", VKNXBLIND_PREFIX);
	}

	printk(VKNXBLIND_PREFIX "using %s for uart\n", port_name);

	/* init and setting up the uart used by kBerry*/
	vknxblind_init_uart(vknxblind_priv, port_name);

	vknxblind_reset_kBerry(vknxblind_priv);
	// vknxblind_down_blind();

	//thread used to test the end of run
	kthread_run(knxblind_monitor_fn, NULL, "knxblind_monitor_fn");

	printk("%s BACKEND PROBE CALLED", VKNXBLIND_PREFIX);

	DBG(VKNXBLIND_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vknxblind_remove(struct vbus_device *vdev) {
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vknxblind structure for %s\n", __func__,vdev->nodename);
	kfree(vknxblind_priv);
}


void vknxblind_close(struct vbus_device *vdev) {
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vdev->dev);

	DBG(VKNXBLIND_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vknxblind_priv->vknxblind.ring, (&vknxblind_priv->vknxblind.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vknxblind_priv->vknxblind.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vknxblind_priv->vknxblind.ring.sring);
	vknxblind_priv->vknxblind.ring.sring = NULL;
}

void vknxblind_suspend(struct vbus_device *vdev) {

	DBG(VKNXBLIND_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vknxblind_resume(struct vbus_device *vdev) {

	DBG(VKNXBLIND_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vknxblind_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vknxblind_sring_t *sring;
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(&vdev->dev);

	DBG(VKNXBLIND_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vknxblind_priv->vknxblind.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vknxblind_interrupt, NULL, 0, VKNXBLIND_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vknxblind_priv->vknxblind.irq = res;
}

void vknxblind_connected(struct vbus_device *vdev) {

	DBG(VKNXBLIND_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

			if (!vknxblind_start(i))
				continue;

			vknxblind_ring_response_ready()

			vknxblind_notify(i);

			vknxblind_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vknxblinddrv = {
	.probe = vknxblind_probe,
	.remove = vknxblind_remove,
	.close = vknxblind_close,
	.connected = vknxblind_connected,
	.reconfigured = vknxblind_reconfigured,
	.resume = vknxblind_resume,
	.suspend = vknxblind_suspend
};

int vknxblind_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vknxblind,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vknxblind-gen");
#endif

	vdevback_init(VKNXBLIND_NAME, &vknxblinddrv);

	return 0;
}

device_initcall(vknxblind_init);
