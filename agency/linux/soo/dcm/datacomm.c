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

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <soo/sooenv.h>

#include <soo/uapi/dcm.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>
#include <soo/dcm/security.h>

#include <soo/soolink/soolink.h>

#include <soo/core/device_access.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/core/core.h>

/* The main requester descriptor managed by Soolink */
static sl_desc_t *datacomm_sl_desc = NULL;

static bool datacomm_initialized = false;

/**
 * At the moment, we experiment a broadcast (no known recipient) and
 * a fixed prio got from the DCM Core.
 */
void datacomm_send(void *ME_buffer, uint32_t size, uint32_t prio) {
	if (unlikely(!datacomm_initialized))
		BUG();

	sl_send(datacomm_sl_desc, ME_buffer, size, 0, prio);
}

/**
 * This function is synchronous and blocks until an incoming ME is available.
 */
static void datacomm_recv(void **ME_buffer, uint32_t *size_p) {
	uint32_t size;
	void *priv_buffer = NULL;

	if (unlikely(!datacomm_initialized))
		BUG();

	size = sl_recv(datacomm_sl_desc, &priv_buffer);

	*ME_buffer = priv_buffer;
	*size_p = size;

}

static int recv_thread_task_fn(void *data) {
	void *ME_compressed_buffer, *ME_decompressed_buffer;
	uint32_t compressed_size, decompressed_size;
	int ret;
#ifdef CONFIG_SOO_CORE_ASF
	int size;
	void *ME_decrypt;
#endif
	while (1) {

		/* Receive data from SOOlink */
		datacomm_recv(&ME_compressed_buffer, &compressed_size);

		/* If the decoder has nothing for us... */
		if (!compressed_size)
			continue;

#ifdef CONFIG_SOO_CORE_ASF
		size = security_decrypt(ME_compressed_buffer, compressed_size, &ME_decrypt);
		if (size <= 0)
			continue;

		if ((ret = decompress_data(&ME_decompressed_buffer, ME_decrypt, size)) < 0) {
			/*
			 * If dcm_decompress_ME returns -EIO, this means that the decompressor could not
			 * decompress the ME. We have to discard it.
			 */

			vfree((void *) ME_decompressed_buffer);
			vfree((void *) ME_compressed_buffer);
			kfree(ME_decrypt);
			continue;
		}

		decompressed_size = ret;

		/* Release the original compressed buffer */
		kfree(ME_decrypt);

#else /* !CONFIG_SOO_CORE_ASF */

		decompress_data(&ME_decompressed_buffer, ME_compressed_buffer, compressed_size);

#endif /* !CONFIG_SOO_CORE_ASF */

		decompressed_size = ret;

		/* Release the original compressed buffer */
		vfree((void *) ME_compressed_buffer);

		ret = dcm_ME_rx(ME_decompressed_buffer, decompressed_size);

		/*
		 * If dc_recv_ME returns -ENOMEM, this means that there is no free buffer.
		 * We have to discard the ME and free its buffer ourselves.
		 * Otherwise, the ME buffer will be freed by the dcm_release_ME function.
		 */
		if (ret < 0)
			vfree((void *) ME_decompressed_buffer);
	}

	return 0;
}

extern void soolink_netsimul_init(void);

void datacomm_init(void) {
	struct task_struct *__ts;

	DBG("Registering the DCM with Soolink\n");

	soo_log("[soo:dcm:datacomm] %s: my agency UID is: ", __func__);
	soo_log_printlnUID(current_soo->agencyUID);

	/*
	 * By default, we are using the WLAN plugin on the MERIDA board, or the
	 * Ethernet otherwise. This behaviour might be changed later.
	 */

#if 1 /* Must be disabled for debugging purposes */

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	datacomm_sl_desc = sl_register(SL_REQ_DCM, SL_IF_WLAN, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	datacomm_sl_desc = sl_register(SL_REQ_DCM, SL_IF_ETH, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_SIMULATION)
	datacomm_sl_desc = sl_register(SL_REQ_DCM, SL_IF_SIM, SL_MODE_UNIBROAD);
#else
#error "SOOlink pluging not configured..."
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	__ts = kthread_create(recv_thread_task_fn, NULL, "datacomm_recv");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);
	wake_up_process(__ts);

#endif
	datacomm_initialized = true;

}
