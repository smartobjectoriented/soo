/*
 * Copyright (C) 2017 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>
#include <soo/dcm/security.h>

#include <soo/uapi/dcm.h>

#include <soo/core/device_access.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/core/core.h>

/* Protection of the buffers */
static struct mutex recv_lock;

LIST_HEAD(dcm_rx_buffers);
static uint32_t buffers_rx_count = 0;

/* Buffer management. The following functions must be called in a context protected by locks. */

/**
 * Perform the sending of ME.
 * Since the Agency Core did a test before sending in order to make sure it was possible to send a ME, at this point,
 * the situation did not change.
 */
static void dcm_send_ME(unsigned long arg) {
	dcm_buffer_t *dcm_buffer = (dcm_buffer_t *) arg;
	void *ME_data;
	uint32_t size = 0;

#ifdef CONFIG_SOO_CORE_ASF
	void *ME_crypt;
#endif

	if (dcm_buffer->ME_data == NULL) {
		datacomm_send(NULL, 0, 0);

		return ;
	}

	/* Check for end of transmission. */
	size = compress_data(&ME_data, dcm_buffer->ME_data, dcm_buffer->ME_size);

#ifdef CONFIG_SOO_CORE_ASF
	/* ME encryption */
	size = security_encrypt(ME_data, size, &ME_crypt);

	/* Prio is not supported yet. Default value: 0. */
	datacomm_send(ME_crypt, size, 0);
#else
	/* Prio is not supported yet. Default value: 0. */
	soo_log("[soo:dcm] Sending to datacomm...\n");

	datacomm_send(ME_data, size, 0);

	soo_log("[soo:dcm] After sending to datacomm...\n");
#endif

	/* Free the compressed ME area. */
	vfree((void *) ME_data);

	/* Free the original buffer */
	vfree((void *) dcm_buffer->ME_data);

#ifdef CONFIG_SOO_CORE_ASF
	/* Free the encrypted buffer */
	kfree(ME_crypt);
#endif

}

/* ME receival */

static int dcm_rx_available_ME(void) {
	int count = 0;
	struct list_head *cur;

	mutex_lock(&recv_lock);

	list_for_each(cur, &dcm_rx_buffers)
		count++;

	mutex_unlock(&recv_lock);

	return count;
}

/**
 * Retrieve a ME to process by the Agency Core subsystem (Migration Manager)
 */
static long dcm_recv_ME(unsigned long arg) {
	dcm_buffer_t *dcm_buffer = (dcm_buffer_t *) arg;
	dcm_buffer_entry_t *dcm_buffer_entry;

	mutex_lock(&recv_lock);

	/* Set to zero */
	memset(dcm_buffer, 0, sizeof(dcm_buffer_t));

	if (!list_empty(&dcm_rx_buffers)) {
		dcm_buffer_entry = list_first_entry(&dcm_rx_buffers, dcm_buffer_entry_t, list);

		*dcm_buffer = dcm_buffer_entry->dcm_buffer;
	}

	mutex_unlock(&recv_lock);

	/* No buffer available args.ME_data = NULL */

	return 0;
}

/**
 * This function looks for a free buffer to process the new incoming ME (issued from Datacomm)
 * If a buffer is found, the function returns the index of the newly reserved buffer (1 or 2).
 * The buffer buffer will be freed by the dcm_release_ME function.
 * If there is no free buffer, the function returns -ENOMEM. The caller must free
 * the ME buffer itself.
 *
 * @param ME_buffer	ME buffer (including info_transfer header) received from datacomm
 * @param size		Size of the ME buffer
 * @return		0 if the buffer has been inserted, or -1 if no place.
 */
int dcm_ME_rx(void *ME_buffer, uint32_t size) {
	dcm_buffer_entry_t *dcm_buffer_entry;

	mutex_lock(&recv_lock);

	soo_log("[soo:dcm] Got a ME buffer rx size: %x bytes\n", size);

	if (buffers_rx_count == DCM_N_RECV_BUFFERS) {
		mutex_unlock(&recv_lock);
		return -1;
	}

	dcm_buffer_entry = kmalloc(sizeof(dcm_buffer_entry_t), GFP_KERNEL);
	BUG_ON(!dcm_buffer_entry);

	dcm_buffer_entry->dcm_buffer.ME_data = ME_buffer;
	dcm_buffer_entry->dcm_buffer.ME_size = ((ME_info_transfer_t *) ME_buffer)->ME_size;

	list_add_tail(&dcm_buffer_entry->list, &dcm_rx_buffers);
	buffers_rx_count++;

	mutex_unlock(&recv_lock);

	return 0;
}

/**
 * Release the buffer of the received and processed ME.
 *
 * @param arg
 */
static long dcm_release_ME(unsigned long arg) {
	dcm_buffer_t *dcm_buffer = (dcm_buffer_t *) arg;
	dcm_buffer_entry_t *dcm_buffer_entry, *tmp;

	mutex_lock(&recv_lock);

	list_for_each_entry_safe(dcm_buffer_entry, tmp, &dcm_rx_buffers, list) {

		if (dcm_buffer_entry->dcm_buffer.ME_data == dcm_buffer->ME_data) {
			list_del(&dcm_buffer_entry->list);
			vfree(dcm_buffer_entry->dcm_buffer.ME_data);
			kfree(dcm_buffer_entry);

			buffers_rx_count--;

			mutex_unlock(&recv_lock);

			return 0;
		}
	}

	/* Shoud never happend */
	BUG();
}

/* /dev interface */

static int dcm_open(struct inode *inode, struct file *file) {
	return 0;
}

static int soolink_dump_neighbourhood(unsigned long arg) {
	uint32_t neigh_bitmap;
	int ret;

	if ((ret = copy_from_user(&neigh_bitmap, (void *) arg, sizeof(uint32_t))) < 0) {
		lprintk("Error when retrieving args (%d)\n", ret);
		BUG();
	}

	/* If prio is to -1, we simply dump the list of neighbours */
	if (neigh_bitmap == -1) {
		soo_log("[soo:dcm] Agency UID: ");
		soo_log_printlnUID(current_soo->agencyUID);

		discovery_dump_neighbours();
	}

	return 0;
}

static long dcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	switch (cmd) {

	case DCM_IOCTL_NEIGHBOUR_COUNT:
		return sl_neighbour_count();

	case DCM_IOCTL_SEND:
		dcm_send_ME(arg);
		return 0;

	case DCM_IOCTL_RX_AVAILABLE_ME_N:
		return dcm_rx_available_ME();

	case DCM_IOCTL_RECV:
		return dcm_recv_ME(arg);

	case DCM_IOCTL_RELEASE:
		return dcm_release_ME(arg);

	case DCM_IOCTL_DUMP_NEIGHBOURHOOD:
		return soolink_dump_neighbourhood(arg);

	case DCM_IOCTL_SET_AGENCY_UID:
		set_agencyUID((uint64_t) arg);
		return 0;

	default:
		lprintk("%s: DCM ioctl %d unavailable...\n", __func__, cmd);
		panic("DCM core");
	}

	return -EINVAL;
}

static struct file_operations dcm_fops = {
	.owner          = THIS_MODULE,
	.open           = dcm_open,
	.unlocked_ioctl = dcm_ioctl,
};

/**
 * DCM initialization function.
 */
int dcm_init(void) {
	int rc;

	DBG("DCM subsys initializing ...\n");

	compressor_init();

	mutex_init(&recv_lock);

	/* Registering device */
	rc = register_chrdev(DCM_MAJOR, DCM_DEV_NAME, &dcm_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", DCM_MAJOR);
		BUG();
	}

	datacomm_init();

	return 0;
}

