/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017 Baptiste Delporte <bonel@bonel.net>
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

#include <stdarg.h>

#include <linux/unistd.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/kthread.h>
#include <linux/rwsem.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/vmacache.h>

#include <linux/ipipe_base.h>
#include <xenomai/rtdm/driver.h>

#include <asm/cacheflush.h>
#include <soo/vbstore.h>
#include <soo/hypervisor.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>
#include <soo/uapi/soo.h>
#include <soo/evtchn.h>

#include <soo/uapi/avz.h>

struct rtdm_vbs_handle rtdm_vbs_state;

LIST_HEAD(watches_rt);
static LIST_HEAD(vbus_msg_standby_list);

static uint32_t vbus_msg_ID = 0;

volatile struct vbstore_domain_interface *__intf_rt;

static unsigned int count_strings(const char *strings, unsigned int len)
{
	unsigned int num;
	const char *p;

	for (p = strings, num = 0; p < strings + len; p += strlen(p) + 1)
		num++;

	return num;
}

static char *kvasprintf_rt(const char *fmt, va_list ap)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = rtdm_malloc(len+1);
	if (!p)
		return NULL;

	vsnprintf(p, len+1, fmt, ap);

	return p;
}

static char *kasprintf_rt(const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf_rt(fmt, ap);
	va_end(ap);

	return p;
}


/* Return the path to dir with /name appended. Buffer must be kfree()'ed. */
static char *join(const char *dir, const char *name)
{
	char *buffer;

	if (strlen(name) == 0)
		buffer = kasprintf_rt("%s", dir);
	else
		buffer = kasprintf_rt("%s/%s", dir, name);

	if (!buffer)
		BUG();

	return buffer;
}

static char **split(char *strings, unsigned int len, unsigned int *num)
{
	char *p, **ret;

	/* Count the strings. */
	*num = count_strings(strings, len);

	/* Transfer to one big alloc for easy freeing. */
	ret = rtdm_malloc(*num * sizeof(char *) + len);
	if (!ret)
		BUG();

	memcpy(&ret[*num], strings, len);
	rtdm_free(strings);

	strings = (char *)&ret[*num];
	for (p = strings, *num = 0; p < strings + len; p += strlen(p) + 1) {
		ret[(*num)++] = p;
	}

	return ret;
}

/* Simplified version of vbs_talkv: single message. */
static void *vbs_single(struct vbus_transaction t, vbus_msg_type_t type, const char *string, unsigned int *len)
{
	msgvec_t vec;

	vec.base = (void *) string;
	vec.len = strlen(string) + 1;

	return vbs_talkv_rt(t, type, &vec, 1, len);
}

char **vbus_directory_rt(struct vbus_transaction t, const char *dir, const char *node, unsigned int *num)
{
	char *strings, *path;
	unsigned int len;
	char **rr;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	strings = vbs_single(t, VBS_DIRECTORY, path, &len);
	rtdm_free(path);

	if (IS_ERR(strings))
		BUG();

	rr = split(strings, len, num);

	return rr;
}

/* Get the value of a single file.
 * Returns a kmalloced value: call free() on it after use.
 * len indicates length in bytes.
 */
void *vbus_read_rt(struct vbus_transaction t, const char *dir, const char *node, unsigned int *len)
{
	char *path;
	void *ret;

	path = join(dir, node);

	ret = vbs_single(t, VBS_READ, path, len);

	rtdm_free(path);

	return ret;
}

/* Write the value of a single file.
 * Returns -err on failure.
 */
void vbus_write_rt(struct vbus_transaction t, const char *dir, const char *node, const char *string)
{
	char *path;
	msgvec_t vec[2];
	char *str;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	vec[0].base = (void *) path;
	vec[0].len = strlen(path) + 1;

	vec[1].base = (void *) string;
	vec[1].len = strlen(string) + 1;

	str = vbs_talkv_rt(t, VBS_WRITE, vec, ARRAY_SIZE(vec), NULL);

	if (IS_ERR(str))
		BUG();
	if (str)
		rtdm_free(str);

	rtdm_free(path);
}

/* Takes tuples of names, scanf-style args, and void **, NULL terminated. */
bool rtdm_vbus_gather(struct vbus_transaction t, const char *dir, ...)
{
	va_list ap;
	const char *name;
	int ret = 0;

	va_start(ap, dir);
	while ((ret == 0) && (name = va_arg(ap, char *)) != NULL) {
		const char *fmt = va_arg(ap, char *);
		void *result = va_arg(ap, void *);
		char *p;

		p = vbus_read_rt(t, dir, name, NULL);
		if (IS_ERR(p))
			BUG();

		/* Case if len == 0, meaning that the entry has not been found in vbstore for instance. */
		if (!strlen(p))
			return false;

		if (sscanf(p, fmt, result) == 0)
			BUG();
		rtdm_free(p);
	}
	va_end(ap);

	return true;
}

/* Start a transaction: changes by others will not be seen during this
 * transaction, and changes will not be visible to others until end.
 */
void vbus_transaction_start_rt(struct vbus_transaction *t)
{
	rtdm_mutex_lock(&rtdm_vbs_state.transaction_mutex);

	transactionID++;

	/* We make sure that an overflow does not lead to value 0 */
	if (unlikely(!transactionID))
		transactionID++;

	DBG("Starting transaction ID: %d...\n", transactionID);
	t->id = transactionID;

	rtdm_mutex_unlock(&rtdm_vbs_state.transaction_mutex);

}

/**
 * vbs_write - low level write
 * @data: buffer to send
 * @len: length of buffer
 *
 * Returns 0 on success, error otherwise.
 */
static void vbs_write(const void *data, unsigned len)
{
	VBSTORE_RING_IDX prod;
	volatile char *dst;

	DBG("__intf_rt->req_prod: %d __intf_rt->req_pvt: %d __intf_rt->req_cons: %d\n", __intf_rt->req_prod, __intf_rt->req_pvt, __intf_rt->req_cons);

	/* Read indexes, then verify. */
	prod = __intf_rt->req_pvt;

	/* Check if we are at the end of the ring, i.e. there is no place for len bytes */
	if (prod + len >= VBSTORE_RING_SIZE) {
		prod = __intf_rt->req_pvt = 0;
		if (prod + len > __intf_rt->req_cons)
			BUG();
	}

#warning assert that the ring is not full

	dst = &__intf_rt->req[prod];

	/* Must write data /after/ reading the producer index. */
	mb();

	memcpy((void *) dst, data, len);

	__intf_rt->req_pvt += len;

	/* Other side must not see new producer until data is there. */
	mb();

}

static void vbs_read(void *data, unsigned len)
{
	VBSTORE_RING_IDX cons;
	volatile const char *src;

	DBG("__intf_rt->rsp_prod: %d __intf_rt->rsp_pvt: %d __intf_rt->rsp_cons: %d\n", __intf_rt->rsp_prod, __intf_rt->rsp_pvt, __intf_rt->rsp_cons);

	/* Read indexes, then verify. */
	cons = __intf_rt->rsp_cons;

	if (cons + len >= VBSTORE_RING_SIZE)
		cons = __intf_rt->rsp_cons = 0;

	src = &__intf_rt->rsp[cons];

	/* Must read data /after/ reading the producer index. */
	mb();

	memcpy(data, (void *) src, len);

	__intf_rt->rsp_cons += len;

	/* Other side must not see free space until we've copied out */
	mb();

}

void *vbs_talkv_rt(struct vbus_transaction t, vbus_msg_type_t type, const msgvec_t *vec, unsigned int num_vecs, unsigned int *len)
{
	vbus_msg_t msg;
	unsigned int i;
	char *payload	= NULL;
	char *__payload_pos = NULL;

	/* Interrupts must be enabled because we expect an (asynchronous) reply from the peer.*/
	BUG_ON(hard_irqs_disabled());

	rtdm_mutex_lock(&rtdm_vbs_state.request_mutex);

	/* Message unique ID (on 32 bits) - 0 is a valid ID */
	msg.id = vbus_msg_ID++;

	msg.transactionID = t.id;

	/* Allocating a completion structure for this message - need to do that dynamically since the msg is part of the shared vbstore page
	 * and the size may vary depending on SMP is enabled or not.
	 */
	msg.u.reply_wait_rt = (rtdm_event_t *) rtdm_malloc(sizeof(rtdm_event_t));

	rtdm_event_init(msg.u.reply_wait_rt, 0);

	msg.type = type;
	msg.len = 0;
	for (i = 0; i < num_vecs; i++)
		msg.len += vec[i].len;

	vbs_write(&msg, sizeof(msg));

	payload = rtdm_malloc(msg.len);
	if (payload == NULL) {
		lprintk("%s:%d ERROR cannot rtdm_alloc the msg payload.\n", __func__, __LINE__);
		BUG();
	}
	memset(payload, 0, msg.len);

	__payload_pos = payload;
	for (i = 0; i < num_vecs; i++) {
		memcpy(__payload_pos, vec[i].base, vec[i].len);
		__payload_pos += vec[i].len;
	}

	DBG("Msg type: %d msg len: %d   content: %s\n", msg.type, msg.len, payload);

	vbs_write(payload, msg.len);

	rtdm_free(payload);

	/* Store the current vbus_msg into the standby list for waiting the reply from vbstore. */

	spin_lock(&rtdm_vbs_state.msg_list_lock);
	list_add_tail(&msg.list, &vbus_msg_standby_list);
	spin_unlock(&rtdm_vbs_state.msg_list_lock);

	__intf_rt->req_prod = __intf_rt->req_pvt;

	mb();

	notify_remote_via_evtchn(__intf_rt->levtchn);

	/* Now we are waiting for the answer from vbstore */
	DBG("Now, we wait for the reply / msg ID: %d (0x%lx)\n", msg.id, &msg.list);

	rtdm_event_wait(msg.u.reply_wait_rt);

	DBG("Talkv protocol completed / reply: %lx\n", msg.reply);

	/* Consistency check */
	if ((msg.reply->type != msg.type) || (msg.reply->id != msg.id)) {
		lprintk("%s: reply msg type or ID does not match...\n", __func__);
		lprintk("VBus received type [%d] expected: %d, received ID [%d] expected: %d\n", msg.reply->type, msg.type, msg.reply->id, msg.id);
		BUG();
	}

	payload = msg.reply->payload;
	if (len != NULL)
		*len = msg.reply->len;

	/* Free the reply msg */
	rtdm_free(msg.reply);

	rtdm_free(msg.u.reply_wait_rt);

	rtdm_mutex_unlock(&rtdm_vbs_state.request_mutex);

	return payload;
}

/* Top half ISR for vbstore processing */
int rtdm_vbus_vbstore_isr(rtdm_irq_t *unused)
{
	vbus_msg_t *msg, *orig_msg, *orig_msg_tmp;
	struct vbus_watch *__w;
	bool found;

	while (__intf_rt->rsp_cons != __intf_rt->rsp_prod) {

		msg = rtdm_malloc(sizeof(vbus_msg_t));
		if (msg == NULL)
			BUG();
		memset(msg, 0, sizeof(vbus_msg_t));

		/* Read the vbus_msg to process */

		vbs_read(msg, sizeof(vbus_msg_t));

		if (msg->len > 0) {
			msg->payload = rtdm_malloc(msg->len);
			if (msg->payload == NULL)
				BUG();

			memset(msg->payload, 0, msg->len);

			vbs_read(msg->payload, msg->len);

		} else
			/* Override previous value */
			msg->payload = NULL;

		if (msg->type == VBS_WATCH_EVENT) {

			found = false;

			spin_lock(&rtdm_vbs_state.watch_list_lock);

			list_for_each_entry(__w, &watches_rt, list) {

				if (!strcmp(__w->node, msg->payload)) {
					__w->pending = true;
					found = true;
					mb();
				}
			}

			if (found)
				rtdm_event_signal(&rtdm_vbs_state.watch_wait);

			spin_unlock(&rtdm_vbs_state.watch_list_lock);

			rtdm_free(msg->payload);
			rtdm_free(msg);


		} else {

			/* Look for the peer vbus_msg which did the request */
			found = false;

			spin_lock(&rtdm_vbs_state.msg_list_lock);

			list_for_each_entry_safe(orig_msg, orig_msg_tmp, &vbus_msg_standby_list, list) {
				if (orig_msg->id == msg->id) {
					list_del(&orig_msg->list); /* Remove this message from the standby list */

					found = true;
					orig_msg->reply = msg;
					mb();

					/* Wake up the thread waiting for the answer */
					rtdm_event_signal(orig_msg->u.reply_wait_rt);

					break; /* Ending the search loop */
				}
			}
			spin_unlock(&rtdm_vbs_state.msg_list_lock);

			if (!found) /* A pending message MUST exist */
				BUG();

		}

	}
	BUG_ON(!hard_irqs_disabled());

	return RTDM_IRQ_HANDLED;
}

/*
 * Main loop to monitor RT watches
 */
void rtdm_vbus_watch_monitor(void)
{
	struct vbus_watch *__w;
	bool found;

	/* Entering the watch monitor loop */

	for (;;) {

		rtdm_event_wait(&rtdm_vbs_state.watch_wait);

		/* Avoiding to be suspended during SOO callback processing... */
		rtdm_mutex_lock(&rtdm_vbs_state.watch_mutex);

		spin_lock(&rtdm_vbs_state.watch_list_lock);

		do {
			found = false;

			list_for_each_entry(__w, &watches_rt, list) {

				/* Is the watch is pending ? */
				if (__w->pending) {

					found = true;
					__w->pending = false;

					spin_unlock(&rtdm_vbs_state.watch_list_lock);
					__w->callback(__w);
					spin_lock(&rtdm_vbs_state.watch_list_lock);

					/* We go out of loop since a callback operation could manipulate the watch list */
					break;

				}
			}
		} while (found);

		spin_unlock(&rtdm_vbs_state.watch_list_lock);

		rtdm_mutex_unlock(&rtdm_vbs_state.watch_mutex);
	}

}

