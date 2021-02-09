/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <soo/netsimul.h>

#include <soo/core/device_access.h>

#include <soo/soolink/transcoder.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/datalink.h>

#include <soo/debug/bandwidth.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>

struct soo_transcoder_env {
	struct mutex coder_tx_lock;

	/*
	 * The decoder is currently able to manage only one block at a time, i.e. it can deserve one call to decoder_recv() on a specific incoming block,
	 * but not more at the moment.
	 */

	struct list_head block_list;
	struct mutex decoder_lock;
	bool decoder_recv_requested;
	decoder_block_t *last_block;

};

/*
 * Create a new block based on the received data.
 */
static decoder_block_t *create_block(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	DBG("Agency UID: ");
	DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

	block = (decoder_block_t *) kmalloc(sizeof(decoder_block_t), GFP_ATOMIC);
	if (!block) {
		lprintk("Cannot allocate memory for a new block!");
		BUG();
	}

	block->sl_desc = sl_desc;

	block->incoming_block = NULL;
	block->size = 0;
	block->cur_pos = NULL;
	block->block_ext_in_progress = false;
	block->discarded_block = false;
	block->complete = false;
	block->last_timestamp = ktime_to_ms(ktime_get());

	list_add_tail(&block->list, &current_soo_transcoder->block_list);

	return block;
}

static decoder_block_t *get_block_by_sl_desc(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	soo_log("[soo:soolink:transcoder] Agency UID: ");
	soo_log_printUID(&sl_desc->agencyUID_from);

	list_for_each_entry(block, &current_soo_transcoder->block_list, list) {
		if (!memcmp(&block->sl_desc->agencyUID_from, &sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE)) {
			soo_log(" Block found\n");
			return block;
		}
	}

	soo_log(" Block not found\n");
	return NULL;
}

/*
 * Receive data over the interface which is specified in the sl_desc descriptor
 */
int decoder_recv(sl_desc_t *sl_desc, void **data) {
	size_t size;

	mutex_lock(&current_soo_transcoder->decoder_lock);
	current_soo_transcoder->decoder_recv_requested = true;
	mutex_unlock(&current_soo_transcoder->decoder_lock);

	/* We still need to manage a timeout according to the specification */
	wait_for_completion(&sl_desc->recv_event);

	size = sl_desc->incoming_block_size;

	/*
	 * Copy received data so that the Decoder cannot free a buffer being used.
	 * The buffer will be freed by the consumer after use.
	 */
	if (size) {
		*data = vmalloc(size);
		BUG_ON(!data);

		memcpy(*data, sl_desc->incoming_block, size);

		mutex_lock(&current_soo_transcoder->decoder_lock);

		/* Free the buffer by the Decoder's side */
		vfree(sl_desc->incoming_block);

	}
	else {
		mutex_lock(&current_soo_transcoder->decoder_lock);
		*data = NULL;
	}

	current_soo_transcoder->decoder_recv_requested = false;

	mutex_unlock(&current_soo_transcoder->decoder_lock);

	return size;
}

int decoder_rx(sl_desc_t *sl_desc, void *data, size_t size) {
	transcoder_packet_t *pkt;
	decoder_block_t *block;

	/* Bypass the Decoder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP) || (sl_desc->req_type == SL_REQ_PEER)) {
		pkt = (transcoder_packet_t *) data;

		/* Allocate the memory for this new (simple) block */
		sl_desc->incoming_block = vmalloc(size - sizeof(transcoder_packet_format_t));

		/* Transfer the block frame */
		memcpy(sl_desc->incoming_block, pkt->payload, size - sizeof(transcoder_packet_format_t));
		sl_desc->incoming_block_size = size - sizeof(transcoder_packet_format_t);

		complete(&sl_desc->recv_event);

		return 0;
	}

	mutex_lock(&current_soo_transcoder->decoder_lock);

	/*
	 * Look for the block associated to this agency UID.
	 */
	if (!(block = get_block_by_sl_desc(sl_desc))) {
		/* If the block does not exist, create it */
		block = create_block(sl_desc);
	}

	if (!block)
		BUG();

	/* Update the slot timestamp */
	block->last_timestamp = ktime_to_ms(ktime_get());

	/* Check the kind of packet we have */
	pkt = (transcoder_packet_t *) data;

	if (pkt->u.simple.consistency_type == CODER_CONSISTENCY_SIMPLE) {

		/* Allocate the memory for this new (simple) block */
		block->incoming_block = vmalloc(size - sizeof(transcoder_packet_format_t));

		/* Transfer the block frame */
		memcpy(block->incoming_block, pkt->payload, size - sizeof(transcoder_packet_format_t));
		block->size = size - sizeof(transcoder_packet_format_t);

		sl_desc->incoming_block_size = size - sizeof(transcoder_packet_format_t);
		sl_desc->incoming_block = block->incoming_block;

		/* If a complete block has been received with no request coming from the requester, free it */
		if (!current_soo_transcoder->decoder_recv_requested) {
			vfree(block->incoming_block);
			block->incoming_block = NULL;

			receiver_cancel_rx(block->sl_desc);

			/* Release the lock on the block processing */
			mutex_unlock(&current_soo_transcoder->decoder_lock);

			return 0;
		}

		complete(&sl_desc->recv_event);

	} else {

		/* At the moment... */
		BUG_ON(size > sizeof(transcoder_packet_format_t) + SL_PACKET_PAYLOAD_MAX_SIZE);

		/* If we missed a packet for this block, we simply discard all subsequent packets */
		if (block->discarded_block) {

			printk("[soo:soolink:decoder] Ignore: %d\n", pkt->u.ext.packetID);

			block->n_recv_packets++;

			/*
			 * Wait for the next packet whose packet ID is 1, meaning that this is the first
			 * packet of the next block
			 */
			if (pkt->u.ext.packetID == 1) {
				block->discarded_block = false;
				block->block_ext_in_progress = false;
			}
			else {
				/* Release the lock on the block processing */
				mutex_unlock(&current_soo_transcoder->decoder_lock);

				return 0;
			}
		}

		if ((block->block_ext_in_progress) && (pkt->u.ext.packetID != block->cur_packetID+1)) {

			printk("[soo:soolink:decoder] Discard current packetID: %d expected: %d", pkt->u.ext.packetID, block->cur_packetID+1);

			/* If the received packetID is smaller than the expected one, it means
			 * that a frame has been re-sent because the sender did not receive an ack.
			 */
#warning Make sure the received frame matches the existing one.
			if (pkt->u.ext.packetID > block->cur_packetID+1) {

				block->discarded_block = true;
				if (block->incoming_block) {
					vfree(block->incoming_block);
					block->incoming_block = NULL;

					receiver_cancel_rx(block->sl_desc);
				}

				sl_desc->incoming_block = NULL;
				sl_desc->incoming_block_size = 0;

			}

			/*
			 * Release the lock on the block processing.
			 * The caller will see that the block has been discarded because incoming_block is NULL
			 * and incoming_block_size is 0.
			 */
			mutex_unlock(&current_soo_transcoder->decoder_lock);

			return 0;
		}

		if (!block->block_ext_in_progress) {
			if (pkt->u.ext.packetID != 1) {
				DBG("## Missed some packets\n");
				/*
				 * We have missed some packets of a new block when processing the current block.
				 * Wait for the next packet ID = 1 to arrive.
				 */
				mutex_unlock(&current_soo_transcoder->decoder_lock);
				return 0;
			}

			block->discarded_block = false;
			block->total_size = pkt->u.ext.nr_packets * SL_PACKET_PAYLOAD_MAX_SIZE;
			block->real_size = 0;
			block->n_total_packets = pkt->u.ext.nr_packets;
			block->n_recv_packets = 0;
			block->cur_packetID = -1;

			block->incoming_block = vmalloc(block->total_size);
			BUG_ON(block->incoming_block == NULL);

			block->cur_pos = block->incoming_block;
			block->block_ext_in_progress = true;
		}

		block->n_recv_packets++;

		memcpy(block->cur_pos, pkt->payload, pkt->u.ext.payload_length);
		block->cur_pos += pkt->u.ext.payload_length;

		block->real_size += pkt->u.ext.payload_length;
		block->cur_packetID = pkt->u.ext.packetID;


		if (block->cur_packetID == pkt->u.ext.nr_packets) {

			block->size = block->real_size;

			sl_desc->incoming_block_size = block->real_size;
			sl_desc->incoming_block = block->incoming_block;

			current_soo_transcoder->last_block = block;

			/* If a complete block has been received with no request coming from the requester, free it */
			if (!current_soo_transcoder->decoder_recv_requested) {
				vfree(block->incoming_block);
				block->incoming_block = NULL;

				receiver_cancel_rx(block->sl_desc);

				/* Release the lock on the block processing */
				mutex_unlock(&current_soo_transcoder->decoder_lock);

				return 0;
			}

			/*
			 * So, we are ready to forward the block to the Soolink core.
			 */

			/* Inform the synchronous waiter about available data */
			complete(&sl_desc->recv_event);

			block->block_ext_in_progress = false;
		}

	}

	/* Release the lock on the block processing */
	mutex_unlock(&current_soo_transcoder->decoder_lock);

	return 0;
}

/*
 * The following task is used to monitor the activities of allocated blocks.
 * Typically, it might happen that an extended block waits for its subsequent packets and they never arrive.
 * In this case, all received packets are discarded and the blocks are released.
 */
static int decoder_watchdog_task_fn(void *arg)  {
	struct list_head *cur, *tmp;
	decoder_block_t *block;
	s64 current_time;

	while (true) {

		msleep(DECODER_WATCHDOG_TASK_PERIOD_MS);

		current_time = ktime_to_ms(ktime_get());

		mutex_lock(&current_soo_transcoder->decoder_lock);

		/* Look for the blocks that exceed the timeout */
		list_for_each_safe(cur, tmp, &current_soo_transcoder->block_list) {
			block = list_entry(cur, decoder_block_t, list);

			/*
			 * If the packet is complete and it is waiting to be consumed, do not do anything.
			 * The consumer will free the buffer.
			 */
			if ((!block->block_ext_in_progress) || (block->complete))
				continue;

			if (current_time - block->last_timestamp >= SOOLINK_DECODE_BLOCK_TIMEOUT) {
				DBG("Discard the block: ");
				DBG_BUFFER(&block->sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				list_del(cur);

				/*
				 * Free the memory allocated for this block.
				 * The buffer should not be freed if the packet has been discarded and it has already been freed.
				 */
				if (block->incoming_block) {
					vfree(block->incoming_block);
					block->incoming_block = NULL;

					receiver_cancel_rx(block->sl_desc);
				}

				kfree(block);
			}
		}

		mutex_unlock(&current_soo_transcoder->decoder_lock);
	}

	return 0;
}

/**
 * Send data according to requirements based on the sl_desc descriptor and performs
 * consistency algorithms/packet splitting if required.
 */
void coder_send(sl_desc_t *sl_desc, void *data, size_t size) {
	transcoder_packet_t *pkt;
	uint32_t packetID, nr_packets;
	bool completed;

	soo_log("[soo:soolink:transcoder] Sending sending %d bytes...\n", size);

	/* Bypass the Coder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP) || (sl_desc->req_type == SL_REQ_PEER)) {
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + size, GFP_ATOMIC);

		/* In fact, do not care about the consistency_type field */
		pkt->u.simple.consistency_type = CODER_CONSISTENCY_SIMPLE;
		memcpy(pkt->payload, data, size);

		/* Forward the packet to the Transceiver */
		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + size, true);

		kfree(pkt);

		return ;
	}

	/* End of transmission ? */
#warning sending all MEs at each time ?
	if (!data) {
		sender_tx(sl_desc, NULL, 0, true);
		return ;
	}

	/*
	 * Take the lock for managing the block and packets.
	 * Protecting the access to the global transID counter.
	 */
	mutex_lock(&current_soo_transcoder->coder_tx_lock);

	/* Check if the block has to be split into multiple packets */
	if (size <= SL_PACKET_PAYLOAD_MAX_SIZE) {
		DBG("Simple packet\n");

		/* Create the simple packet */
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + size, GFP_ATOMIC);

		pkt->u.simple.consistency_type = CODER_CONSISTENCY_SIMPLE;
		memcpy(pkt->payload, data, size);

		/* We forward the packet to the Transceiver. The size at the reception
		 * will be taken from the plugin (RX).
		 */

		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + size, true);

		kfree(pkt);

	} else {

		/* Determine the number of packets required for this block */
		nr_packets = DIV_ROUND_UP(size, SL_PACKET_PAYLOAD_MAX_SIZE);

		DBG("Extended packet, nr_packets=%d\n", nr_packets);

		/* Need to iterate over multiple packets */
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + SL_PACKET_PAYLOAD_MAX_SIZE, GFP_ATOMIC);
		BUG_ON(!pkt);

		for (packetID = 1; packetID < nr_packets + 1; packetID++) {

			/* Tell Datalink that this is the last packet in the block */
			completed = (packetID == nr_packets);

			/* Create an extended packet */

			pkt->u.ext.consistency_type = CODER_CONSISTENCY_EXT;
			pkt->u.ext.nr_packets = nr_packets;

			pkt->u.ext.packetID = packetID;
			pkt->u.ext.payload_length = ((size > SL_PACKET_PAYLOAD_MAX_SIZE) ? SL_PACKET_PAYLOAD_MAX_SIZE : size);

			memcpy(pkt->payload, data, pkt->u.ext.payload_length);
			data += pkt->u.ext.payload_length;
			size -= pkt->u.ext.payload_length;

			/*
			 * We forward the packet to the Transceiver. The size at the reception
			 * will be taken from the plugin (RX).
			 */

			if (sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + pkt->u.ext.payload_length, completed) < 0) {
				/* There has been something wrong with Datalink. Abort the transmission of the block. */
				break;
			}
		}

		kfree(pkt);
	}

	/* Finally ... */
	mutex_unlock(&current_soo_transcoder->coder_tx_lock);

	soo_log("[soo:soolink:transcoder] Send completed.\n");
}

/**
 * Initialize the Coder functional block of Soolink.
 */
void transcoder_init(void) {
	struct task_struct *__ts;

	current_soo->soo_transcoder = kzalloc(sizeof(struct soo_transcoder_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_transcoder);

	current_soo_transcoder->decoder_recv_requested = false;
	current_soo_transcoder->last_block = NULL;

	mutex_init(&current_soo_transcoder->coder_tx_lock);
	INIT_LIST_HEAD(&current_soo_transcoder->block_list);

	mutex_init(&current_soo_transcoder->decoder_lock);

	/* Watchdog */
	__ts = kthread_create(decoder_watchdog_task_fn, NULL, "decoder_watchdog_task");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);

	wake_up_process(__ts);

}
