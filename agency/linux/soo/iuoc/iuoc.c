/*
 * Copyright (C) 2023-2024 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#include <stdarg.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h> /* register_chrdev, unregister_chrdev */
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <soo/iuoc/iuoc.h>
#include <soo/uapi/iuoc.h>


static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;

iuoc_data_t *me_received;

static void forward_data(iuoc_data_t iuoc_data);

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - open was called!\n");
	return 0;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("ioctl_example - close was called!\n");
	return 0;
}

/* Global Variable for reading and writing */
static long int my_ioctl(struct file *file, unsigned cmd, unsigned long arg) { 

	iuoc_data_t iuoc_data;
	soo_blind_data_t blind_data;

	switch(cmd) {
	case UIOC_IOCTL_SEND_DATA:
		if(copy_from_user(&iuoc_data, (iuoc_data_t *) arg, sizeof(iuoc_data))) {
			printk("UIOC_IOCTL_SEND_DATA - Error copying data from user!\n");
		}
		forward_data(iuoc_data);
		break;

	case UIOC_IOCTL_RECV_DATA:
		iuoc_data.me_type = IUOC_ME_BLIND;
		iuoc_data.timestamp = 654321;
		blind_data.action = STORE_DOWN_STEP;
		iuoc_data.data.blind_data = blind_data;
		if(copy_to_user((iuoc_data_t *) arg, &iuoc_data, sizeof(iuoc_data))) 
			printk("ioctl_example - Error copying data to user!\n");
		else
			printk("Sending data to broker\n");
		 break;

	case UIOC_IOCTL_TEST:
		if(copy_from_user(&iuoc_data, (iuoc_data_t *) arg, sizeof(iuoc_data))) 
			printk("ioctl_example - Error copying data from user!\n");
		else
			printk("Greetings from kernel !\n");
		break;
	}
	return 0;
}

struct file_operations iuoc_fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.unlocked_ioctl = my_ioctl
};
 
/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int ModuleInit(void) {

    int device_created = 0;

    if (alloc_chrdev_region(&major, 0, 1, NAME "_proc") < 0) {
		printk("[IUOC] alloc_chrdev_region failed\n");
		BUG();
	}

    if ((myclass = class_create(THIS_MODULE, NAME "_sys")) == NULL) {
		printk("[IUOC] class_create failed\n");
		BUG();
	}

    if (device_create(myclass, NULL, major, NULL, NAME) == NULL) {
		printk("[IUOC] device_create failed\n");
		BUG();	
	}

    device_created = 1;
    cdev_init(&mycdev, &iuoc_fops);
    if (cdev_add(&mycdev, major, 1) == -1) {
		printk("[IUOC] cdev_add failed\n");
		BUG();		
	}

	me_received = kmalloc(sizeof(iuoc_data_t), GFP_KERNEL);
	if(!me_received) {
		printk("[IUOC] kmalloc me_received failed\n");
		BUG();		
	}

	return 0;
}

void forward_data(iuoc_data_t iuoc_data) 
{
	switch (iuoc_data.me_type) {
	case IUOC_ME_BLIND:
		printk("[IUOC] Data sent IUOC_ME_BLIND -> SOO : me_type=%d, timestamp=%d, value=%d\n", 
				iuoc_data.me_type, iuoc_data.timestamp, iuoc_data.data.blind_data.action);
				break;
	case IUOC_ME_SWITCH:
		printk("[IUOC] Data sent IUOC_ME_SWITCH -> SOO : me_type=%d, timestamp=%d, value=%d\n", 
				iuoc_data.me_type, iuoc_data.timestamp, iuoc_data.data.switch_data.action);
				break;
	}
}


late_initcall(ModuleInit);
