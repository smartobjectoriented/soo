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

#include <soo/sooenv.h>

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

	decoder_block_t *last_block;
};

/*
 * When data is incoming, they take place in a block associated to a sl_desc.
 * Depending on the processing speed, several blocks can be received before they
 * are all processed.
 */

/**
 * Count the number of available blocks for a given sl_desc.
 *
 * @param sl_desc
 * @return the number of available blocks
 */
static int block_count(sl_desc_t *sl_desc) {
	decoder_block_t *block;
	int count = 0;

	list_for_each_entry(block, &current_soo_transcoder->block_list, list) {
		if ((sl_desc == block->sl_desc) && block->ready)
			count++;
	}

	return count;
}

/**
 * Create a new block
 *
 * @param sl_desc associated to this block
 * @return the newly allocated block
 */
static decoder_block_t *new_block(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	DBG("Agency UID: ");
	DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

	block = (decoder_block_t *) kzalloc(sizeof(decoder_block_t), GFP_KERNEL);
	if (!block) {
		lprintk("Cannot allocate memory for a new block!");
		BUG();
	}

	block->sl_desc = sl_desc;

	block->last_timestamp = ktime_to_ms(ktime_get());

	list_add_tail(&block->list, &current_soo_transcoder->block_list);

	return block;
}

/**
 * Retrieve a possible existing block already associated to this sl_desc and
 * with this origin (sender).
 *
 * We assume that there may be only one block in progress per sl_desc and per origin.
 *
 * @param sl_desc which also contains the origin (sender).
 * @return existing block or NULL if no block found.
 */
static decoder_block_t *get_block_by_sl_desc(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	soo_log("[soo:soolink:transcoder:block] Agency UID: ");
	soo_log_printUID(sl_desc->agencyUID_from);

	list_for_each_entry(block, &current_soo_transcoder->block_list, list) {
		if ((sl_desc == block->sl_desc) &&
		    (block->sl_desc->agencyUID_from == sl_desc->agencyUID_from) &&
		    !block->ready) {
			soo_log(" Block found\n");
			return block;
		}
	}

	soo_log(" Block not found\n");

	return NULL;
}

/**
 * Find an available block associated to a sl_desc descriptor, whatever its origin.
 *
 * @param sl_desc
 * @return The block which is ready to be processed by the requester.
 */
static decoder_block_t *pick_next_available(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	list_for_each_entry(block, &current_soo_transcoder->block_list, list) {
		if ((sl_desc == block->sl_desc) && block->ready)
			return block;
	}

	/* No block available for this sl_desc */
	return NULL;
}

/*
 * Receive data over the interface which is specified in the sl_desc descriptor
 */
int decoder_recv(sl_desc_t *sl_desc, void **data) {
	decoder_block_t *block;
	uint32_t size;

	/* We still need to manage a timeout according to the specification */
	wait_for_completion(&sl_desc->recv_event);

	mutex_lock(&current_soo_transcoder->decoder_lock);

	/* At least, one block must be available, otherwise the completion has been badly set to true. */
	block = pick_next_available(sl_desc);
	BUG_ON(!block);

	size = block->size;

	/*
	 * Copy received data so that the Decoder cannot free a buffer being used.
	 * The buffer will be freed by the consumer after use.
	 */

	*data = vmalloc(block->size);
	BUG_ON(!data);

	memcpy(*data, block->incoming_block, block->size);

	/* Free the buffer by the Decoder's side */
	vfree(block->incoming_block);

	list_del(&block->list);
	kfree(block);

	mutex_unlock(&current_soo_transcoder->decoder_lock);

	return size;
}

void decoder_rx(sl_desc_t *sl_desc, void *data, uint32_t size) {
	transcoder_packet_t *pkt;
	decoder_block_t *block;

	mutex_lock(&current_soo_transcoder->decoder_lock);

	/*
	 * Look for the block associated to this agency UID.
	 */
	block = get_block_by_sl_desc(sl_desc);
	if (!block) {

		/* We cannot exceed the maximum number of available blocks
		 * for a given sl_desc.
		 */
		if (block_count(sl_desc) == MAX_READY_BLOCK_COUNT) {

			printk("[soo:soolink:decoder] Too many block, discarding the RX\n");
			receiver_cancel_rx(sl_desc);

			goto out;
		}

		/* If the block does not exist, create it */
		block = new_block(sl_desc);
	}

	/* Bypass the Decoder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP) || (sl_desc->req_type == SL_REQ_PEER)) {

		pkt = (transcoder_packet_t *) data;

		/* Allocate the memory for this new (simple) block */
		block->incoming_block = vmalloc(size - sizeof(transcoder_packet_t));

		/* Transfer the block frame */
		memcpy(block->incoming_block, pkt->payload, size - sizeof(transcoder_packet_t));
		block->size = size - sizeof(transcoder_packet_t);

		block->ready = true;

		complete(&sl_desc->recv_event);

		goto out;
	}

	/* Update the slot timestamp */
	block->last_timestamp = ktime_to_ms(ktime_get());

	/* Check the kind of packet we have */
	pkt = (transcoder_packet_t *) data;

	if (pkt->nr_packets == 1) {

		/* Allocate the memory for this new (simple) block */
		block->incoming_block = vmalloc(size - sizeof(transcoder_packet_t));

		/* Transfer the block frame */
		memcpy(block->incoming_block, pkt->payload, size - sizeof(transcoder_packet_t));
		block->size = size - sizeof(transcoder_packet_t);

		/* The block is ready to be processed. */
		block->ready = true;

		complete(&sl_desc->recv_event);

	} else {

		/* At the moment... */
		BUG_ON(size > sizeof(transcoder_packet_t) + SL_PACKET_PAYLOAD_MAX_SIZE);

		if ((block->block_ext_in_progress) && (pkt->packetID != block->cur_packetID+1)) {

			soo_log("[soo:soolink:decoder] Discard current packetID: %d expected: %d", pkt->packetID, block->cur_packetID+1);

			/* If the received packetID is smaller than the expected one, it means
			 * that a frame has been re-sent because the sender did not receive an ack.
			 * We discard the whole block.
			 */

			if (pkt->packetID > block->cur_packetID+1) {

				if (block->incoming_block)
					vfree(block->incoming_block);

				receiver_cancel_rx(block->sl_desc);

				list_del(&block->list);
				kfree(block);

			}

			/*
			 * Release the lock on the block processing.
			 * The caller will see that the block has been discarded because incoming_block is NULL
			 * and incoming_block_size is 0.
			 */

			goto out;
		}

		if (!block->block_ext_in_progress) {
			if (pkt->packetID != 1) {
				soo_log("[soo:soolink:decoder] !! Missed some packets !!\n");
				/*
				 * We have missed some packets of a new block when processing the current block.
				 * Wait for the next packet ID = 1 to arrive.
				 */

				goto out;
			}

			block->size = 0;
			block->cur_packetID = -1;

			block->incoming_block = vmalloc(pkt->nr_packets * SL_PACKET_PAYLOAD_MAX_SIZE);
			BUG_ON(block->incoming_block == NULL);

			block->cur_pos = block->incoming_block;
			block->block_ext_in_progress = true;
		}

		memcpy(block->cur_pos, pkt->payload, pkt->payload_length);

		block->cur_pos += pkt->payload_length;
		block->size += pkt->payload_length;
		block->cur_packetID = pkt->packetID;

		if (block->cur_packetID == pkt->nr_packets) {

			current_soo_transcoder->last_block = block;

			/* Inform the synchronous waiter about available data */

			block->block_ext_in_progress = false;
			block->ready = true;

			complete(&sl_desc->recv_event);
		}

	}

out:
	/* Release the lock on the block processing */
	mutex_unlock(&current_soo_transcoder->decoder_lock);
}

/*
 * The following task is used to monitor the activities of allocated blocks.
 * Typically, it might happen that an extended block waits for its subsequent packets and they never arrive.
 * In this case, all received packets are discarded and the blocks are released.
 */
static int decoder_watchdog_task_fn(void *arg)  {
	decoder_block_t *block, *tmp;
	s64 current_time;

	while (true) {

		msleep(DECODER_WATCHDOG_TASK_PERIOD_MS);

		current_time = ktime_to_ms(ktime_get());

		mutex_lock(&current_soo_transcoder->decoder_lock);

		/* Look for the blocks that exceed the timeout */
		list_for_each_entry_safe(block, tmp, &current_soo_transcoder->block_list, list) {

			if (!block->block_ext_in_progress)
				continue;

			if (current_time - block->last_timestamp >= SOOLINK_DECODE_BLOCK_TIMEOUT) {
				DBG("Discard the block: ");
				DBG_BUFFER(&block->sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				/*
				 * Free the memory allocated for this block.
				 * The buffer should not be freed if the packet has been discarded and it has already been freed.
				 */
				if (block->incoming_block) {
					vfree(block->incoming_block);

					/* Tell the datalink to cancel the receival of data* */
					receiver_cancel_rx(block->sl_desc);
				}

				list_del(&block->list);
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
void coder_send(sl_desc_t *sl_desc, void *data, uint32_t size) {
	transcoder_packet_t *pkt;
	uint32_t packetID, nr_packets;
	bool completed;

	soo_log("[soo:soolink:transcoder] Sending sending %d bytes...\n", size);

	/* Bypass the Coder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP) || (sl_desc->req_type == SL_REQ_PEER)) {
		pkt = kmalloc(sizeof(transcoder_packet_t) + size, GFP_ATOMIC);

		memcpy(pkt->payload, data, size);

		/* Forward the packet to the Transceiver */
		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_t) + size, true);

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

	/* Check if the block needs to be split into multiple packets */
	if (size <= SL_PACKET_PAYLOAD_MAX_SIZE) {
		DBG("Simple packet\n");

		/* Create the simple packet */
		pkt = kzalloc(sizeof(transcoder_packet_t) + size, GFP_ATOMIC);
		BUG_ON(!pkt);

		pkt->nr_packets = 1;

		memcpy(pkt->payload, data, size);

		pkt->payload_length = size;

		/* We forward the packet to the Transceiver. The size at the reception
		 * will be taken from the plugin (RX).
		 */

		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_t) + size, true);

		kfree(pkt);

	} else {

		/* Determine the number of packets required for this block */
		nr_packets = DIV_ROUND_UP(size, SL_PACKET_PAYLOAD_MAX_SIZE);

		DBG("Extended packet, nr_packets=%d\n", nr_packets);

		/* Need to iterate over multiple packets */
		pkt = kzalloc(sizeof(transcoder_packet_t) + SL_PACKET_PAYLOAD_MAX_SIZE, GFP_ATOMIC);
		BUG_ON(!pkt);

		for (packetID = 1; packetID < nr_packets + 1; packetID++) {

			/* Tell Datalink that this is the last packet in the block */
			completed = (packetID == nr_packets);

			/* Create an extended packet */

			pkt->nr_packets = nr_packets;

			pkt->packetID = packetID;
			pkt->payload_length = ((size > SL_PACKET_PAYLOAD_MAX_SIZE) ? SL_PACKET_PAYLOAD_MAX_SIZE : size);

			memcpy(pkt->payload, data, pkt->payload_length);
			data += pkt->payload_length;
			size -= pkt->payload_length;

			/*
			 * We forward the packet to the Transceiver. The size at the reception
			 * will be taken from the plugin (RX).
			 */

			if (sender_tx(sl_desc, pkt, sizeof(transcoder_packet_t) + pkt->payload_length, completed) < 0) {
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
