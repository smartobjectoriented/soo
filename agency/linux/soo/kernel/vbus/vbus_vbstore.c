/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <uapi/linux/sched/types.h>

#include <asm/cacheflush.h>

#include <soo/vbstore.h>
#include <soo/hypervisor.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>
#include <soo/uapi/soo.h>
#include <soo/evtchn.h>

#include <soo/vbus_me.h>

#include <soo/uapi/avz.h>

#include <soo/uapi/soo.h>

#define IRQF_DISABLED	0

struct vbs_handle {
	/* A list of replies. Currently only one will ever be outstanding. */
	struct list_head reply_list;

	/*
	 * Mutex ordering: transaction_mutex -> watch_mutex -> request_mutex.
	 *
	 * transaction_mutex must be held before incrementing
	 * transaction_count. The mutex is held when a suspend is in
	 * progress to prevent new transactions starting.
	 *
	 * When decrementing transaction_count to zero the wait queue
	 * should be woken up, the suspend code waits for count to
	 * reach zero.
	 */

	/* Protect transactions against save/restore. */
	struct mutex transaction_mutex;
	volatile uint32_t transaction_count;

	struct mutex request_mutex;

	struct completion watch_wait;

	/* To manage ongoing transactions when the ME must be suspended */
	struct mutex transaction_group_mutex;

	/* Protect watch (de)register against save/restore. */
	struct mutex watch_mutex;

	spinlock_t msg_list_lock;
};
static struct vbs_handle vbs_state;

/* List of registered watches, and a lock to protect it. */
static LIST_HEAD(watches);
static LIST_HEAD(vbus_msg_standby_list);

static uint32_t vbus_msg_ID = 0;
/*
 * transactionID = 0 means that there is no attached transaction. Hence, when transaction_start() is called for the first time,
 * it will be directly incremented to 1.
 */
unsigned int transactionID = 0;


void vbs_dump_watches(void) {

	struct vbus_watch *w;

	printk("----------- VBstore watches dump --------------\n");

	list_for_each_entry(w, &watches, list)
		printk("  %s\n", w->node);

	printk("--------------- end --------------------\n");

}

/* Local reference to shared vbstore page between us and vbstore */
volatile struct vbstore_domain_interface *__intf;

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

	DBG("__intf->req_prod: %d __intf->req_pvt: %d __intf->req_cons: %d\n", __intf->req_prod, __intf->req_pvt, __intf->req_cons);

	/* Read indexes, then verify. */
	prod = __intf->req_pvt;

	/* Check if we are at the end of the ring, i.e. there is no place for len bytes */
	if (prod + len >= VBSTORE_RING_SIZE) {
		prod = __intf->req_pvt = 0;
		if (prod + len > __intf->req_cons)
			BUG();
	}

#warning assert that the ring is not full

	dst = &__intf->req[prod];

	/* Must write data /after/ reading the producer index. */
	mb();

	memcpy((void *) dst, data, len);

	__intf->req_pvt += len;

	/* Other side must not see new producer until data is there. */
	mb();
}

static void vbs_read(void *data, unsigned len)
{
	VBSTORE_RING_IDX cons;
	volatile const char *src;

	DBG("__intf->rsp_prod: %d __intf->rsp_pvt: %d __intf->rsp_cons: %d\n", __intf->rsp_prod, __intf->rsp_pvt, __intf->rsp_cons);

	/* Read indexes, then verify. */
	cons = __intf->rsp_cons;

	if (cons + len >= VBSTORE_RING_SIZE)
		cons = __intf->rsp_cons = 0;

	src = &__intf->rsp[cons];

	/* Must read data /after/ reading the producer index. */
	mb();

	memcpy(data, (void *) src, len);

	__intf->rsp_cons += len;

	/* Other side must not see free space until we've copied out */
	mb();
}

/*
 * Main function to send a message to vbstore. It is the only way to send a
 * message to vbstore. It follows a synchronous send/reply scheme.
 * A message may have several strings within the payload. These (sub-)strings are known as vectors (msgvec_t).
 */
static void *vbs_talkv(struct vbus_transaction t, vbus_msg_type_t type, const msgvec_t *vec, unsigned int num_vecs, unsigned int *len)
{
	vbus_msg_t msg;
	unsigned int i;
	char *payload	= NULL;
	char *__payload_pos = NULL;
	unsigned long flags;

	/* Check if the request is issued from CPU #1 (RT agency) */
	if (smp_processor_id() == AGENCY_RT_CPU)
		return vbs_talkv_rt(t, type, vec, num_vecs, len);

	/* Interrupts must be enabled because we expect an (asynchronous) reply from the peer.*/
	BUG_ON(hard_irqs_disabled());

	mutex_lock(&vbs_state.request_mutex);

	/* Message unique ID (on 32 bits) - 0 is a valid ID */
	msg.id = vbus_msg_ID++;

	msg.transactionID = t.id;

	/* Allocating a completion structure for this message - need to do that dynamically since the msg is part of the shared vbstore page
	 * and the size may vary depending on SMP is enabled or not.
	 */
	msg.u.reply_wait = (struct completion *) kmalloc(sizeof(struct completion), GFP_ATOMIC);

	init_completion(msg.u.reply_wait);

	msg.type = type;
	msg.len = 0;
	for (i = 0; i < num_vecs; i++)
		msg.len += vec[i].len;

	vbs_write(&msg, sizeof(msg));

	payload = kzalloc(msg.len, GFP_ATOMIC);
	if (payload == NULL) {
		lprintk("%s:%d ERROR cannot kmalloc the msg payload.\n", __func__, __LINE__);
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
	kfree(payload);

	/* Store the current vbus_msg into the standby list for waiting the reply from vbstore. */
	spin_lock_irqsave(&vbs_state.msg_list_lock, flags);

	list_add_tail(&msg.list, &vbus_msg_standby_list);

	spin_unlock_irqrestore(&vbs_state.msg_list_lock, flags);

	__intf->req_prod = __intf->req_pvt;

	mb();

	notify_remote_via_evtchn(__intf->levtchn);

	/* Now we are waiting for the answer from vbstore */
	DBG("Now, we wait for the reply / msg ID: %d (0x%lx)\n", msg.id, &msg.list);

	wait_for_completion(msg.u.reply_wait);

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
	kfree(msg.reply);
	kfree(msg.u.reply_wait);

	mutex_unlock(&vbs_state.request_mutex);

	return payload;
}

/* Simplified version of vbs_talkv: single message. */
static void *vbs_single(struct vbus_transaction t, vbus_msg_type_t type, const char *string, unsigned int *len)
{
	msgvec_t vec;

	vec.base = (void *) string;
	vec.len = strlen(string) + 1;

	return vbs_talkv(t, type, &vec, 1, len);
}

static unsigned int count_strings(const char *strings, unsigned int len)
{
	unsigned int num;
	const char *p;

	for (p = strings, num = 0; p < strings + len; p += strlen(p) + 1)
		num++;

	return num;
}

/* Return the path to dir with /name appended. Buffer must be kfree()'ed. */
static char *join(const char *dir, const char *name)
{
	char *buffer;

	if (strlen(name) == 0)
		buffer = kasprintf(GFP_ATOMIC, "%s", dir);
	else
		buffer = kasprintf(GFP_ATOMIC, "%s/%s", dir, name);

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
	ret = kmalloc(*num * sizeof(char *) + len, GFP_ATOMIC);
	if (!ret)
		BUG();

	memcpy(&ret[*num], strings, len);
	kfree(strings);

	strings = (char *)&ret[*num];
	for (p = strings, *num = 0; p < strings + len; p += strlen(p) + 1) {
		ret[(*num)++] = p;
	}

	return ret;
}

char **vbus_directory(struct vbus_transaction t, const char *dir, const char *node, unsigned int *num)
{
	char *strings, *path;
	unsigned int len;
	char **rr;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	strings = vbs_single(t, VBS_DIRECTORY, path, &len);
	kfree(path);

	if (IS_ERR(strings))
		BUG();

	/*
	 * If the result is empty, no allocation has been done and there is no need fo kfree'd anything.
	 */
	if (len) {
		rr = split(strings, len, num);
		return rr;
	} else
		BUG();

}

/*
 * Check if a directory exists.
 * Return 1 if the directory has been found, 0 otherwise.
 */
int vbus_directory_exists(struct vbus_transaction t, const char *dir, const char *node) {
	char *strings, *path;
	unsigned int len;
	int res;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	strings = vbs_single(t, VBS_DIRECTORY_EXISTS, path, &len);
	kfree(path);

	res = (strings[0] == '1') ? 1 : 0;

	/* Don't forget to free the payload */
	if (smp_processor_id() == AGENCY_CPU)
		kfree(strings);
	else
		rtdm_free(strings);

	return res;
}

/* Get the value of a single file.
 * Returns a kmalloced value: call free() on it after use.
 * len indicates length in bytes.
 */
void *vbus_read(struct vbus_transaction t, const char *dir, const char *node, unsigned int *len)
{
	char *path;
	void *ret;
	if (smp_processor_id() == AGENCY_RT_CPU)
		return vbus_read_rt(t, dir, node, len);

	path = join(dir, node);

	ret = vbs_single(t, VBS_READ, path, len);

	kfree(path);

	return ret;
}

/* Write the value of a single file.
 * Returns -err on failure.
 */
void vbus_write(struct vbus_transaction t, const char *dir, const char *node, const char *string)
{
	const char *path;
	msgvec_t vec[2];
	char *str;

	if (smp_processor_id() == AGENCY_RT_CPU) {
		vbus_write_rt(t, dir, node, string);
		return ;
	}

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	vec[0].base = (void *) path;
	vec[0].len = strlen(path) + 1;

	vec[1].base = (void *) string;
	vec[1].len = strlen(string) + 1;

	str = vbs_talkv(t, VBS_WRITE, vec, ARRAY_SIZE(vec), NULL);

	if (IS_ERR(str))
		BUG();
	if (str)
		kfree(str);

	kfree(path);
}

/* Create a new directory. */
void vbus_mkdir(struct vbus_transaction t, const char *dir, const char *node)
{
	char *path;
	char *str;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	str = vbs_single(t, VBS_MKDIR, path, NULL);
	if (IS_ERR(str))
		BUG();

	if (str) {
		if (smp_processor_id() == AGENCY_RT_CPU)
			rtdm_free(str);
		else
			kfree(str);
	}

	kfree(path);
}

/* Destroy a file or directory (directories must be empty). */
void vbus_rm(struct vbus_transaction t, const char *dir, const char *node)
{
	char *path;
	char *str;

	path = join(dir, node);
	if (IS_ERR(path))
		BUG();

	str = vbs_single(t, VBS_RM, path, NULL);
	if (IS_ERR(str))
		BUG();
	if (str) {
		if (smp_processor_id() == AGENCY_RT_CPU)
			rtdm_free(str);
		else
			kfree(str);
	}

	kfree(path);
}

/* Start a transaction: changes by others will not be seen during this
 * transaction, and changes will not be visible to others until end.
 */
void vbus_transaction_start(struct vbus_transaction *t)
{
	if (smp_processor_id() == AGENCY_RT_CPU) {
		vbus_transaction_start_rt(t);
		return ;
	}

	mutex_lock(&vbs_state.transaction_mutex);

	if (vbs_state.transaction_count == 0)
		mutex_lock(&vbs_state.transaction_group_mutex);

	vbs_state.transaction_count++;

	transactionID++;

	/* We make sure that an overflow does not lead to value 0 */
	if (unlikely(!transactionID))
		transactionID++;

	DBG("Starting transaction ID: %d...\n", transactionID);
	t->id = transactionID;

	mutex_unlock(&vbs_state.transaction_mutex);
}

/*
 * End a transaction.
 * At this moment, pending watch events raised during operations bound to this transaction ID can be sent to watchers.
 */
void vbus_transaction_end(struct vbus_transaction t)
{
	char *str;

	str = vbs_single(t, VBS_TRANSACTION_END, "", NULL);

	if (IS_ERR(str))
		BUG();

	DBG("Ending transaction ID: %d\n", t.id);

	if (str) {
		if (smp_processor_id() == AGENCY_RT_CPU)
			rtdm_free(str);
		else
			kfree(str);
	}

	if (smp_processor_id() == AGENCY_CPU) {
		mutex_lock(&vbs_state.transaction_mutex);

		vbs_state.transaction_count--;

		if (vbs_state.transaction_count == 0)
			mutex_unlock(&vbs_state.transaction_group_mutex);

		mutex_unlock(&vbs_state.transaction_mutex);
	}
}

/* Single read and scanf: returns -errno or num scanned. */
int vbus_scanf(struct vbus_transaction t, const char *dir, const char *node, const char *fmt, ...)
{
	va_list ap;
	int ret;
	char *val;

	DBG("%s(%s, %s)\n", __func__, dir, node);

	val = vbus_read(t, dir, node, NULL);
	if (IS_ERR(val))
		return PTR_ERR(val);

	va_start(ap, fmt);
	ret = vsscanf(val, fmt, ap);
	va_end(ap);

	if (smp_processor_id() == AGENCY_RT_CPU)
		rtdm_free(val);
	else
		kfree(val);

	/* Distinctive errno. */
	if (ret == 0) {
		lprintk("%s: ret=0 for %s, %s\n", __func__, dir, node);
		BUG();
	}

	return ret;
}
EXPORT_SYMBOL_GPL(vbus_scanf);

/* Single printf and write: returns -errno or 0. */
void vbus_printf(struct vbus_transaction t, const char *dir, const char *node, const char *fmt, ...)
{
	va_list ap;
	int ret;
#define PRINTF_BUFFER_SIZE 4096
	char *printf_buffer;

	printf_buffer = kmalloc(PRINTF_BUFFER_SIZE, GFP_ATOMIC);
	if (printf_buffer == NULL)
		BUG();

	va_start(ap, fmt);
	ret = vsnprintf(printf_buffer, PRINTF_BUFFER_SIZE, fmt, ap);
	va_end(ap);

	BUG_ON(ret > PRINTF_BUFFER_SIZE-1);

	vbus_write(t, dir, node, printf_buffer);

	kfree(printf_buffer);
}

/* Takes tuples of names, scanf-style args, and void **, NULL terminated. */
bool vbus_gather(struct vbus_transaction t, const char *dir, ...)
{
	va_list ap;
	const char *name;

	va_start(ap, dir);
	while ((name = va_arg(ap, char *)) != NULL) {
		const char *fmt = va_arg(ap, char *);
		void *result = va_arg(ap, void *);
		char *p;

		p = vbus_read(t, dir, name, NULL);

		if (IS_ERR(p))
			BUG();

		/* Case if len == 0, meaning that the entry has not been found in vbstore for instance. */
		if (!strlen(p))
			return false;

		if (sscanf(p, fmt, result) == 0)
			BUG();

		if (smp_processor_id() == AGENCY_CPU)
			kfree(p);
		else
			rtdm_free(p);

	}
	va_end(ap);

	return true;
}

void vbs_watch(const char *path)
{
	msgvec_t vec[1];

	vec[0].base = (void *) path;
	vec[0].len = strlen(path) + 1;

	if (IS_ERR(vbs_talkv(VBT_NIL, VBS_WATCH, vec, ARRAY_SIZE(vec), NULL)))
		BUG();
}

static void vbs_unwatch(const char *path)
{
	msgvec_t vec[1];

	vec[0].base = (char *) path;
	vec[0].len = strlen(path) + 1;

	if (IS_ERR(vbs_talkv(VBT_NIL, VBS_UNWATCH, vec, ARRAY_SIZE(vec), NULL)))
		BUG();
}

/*
 * Look for an existing watch on a certain node (path). The first one is retrieved.
 * Since there might be several watches with different callbacks and the same node, the
 * first watch entry is returned.
 */
static struct vbus_watch *find_first_watch(struct list_head *__watches, const char *node)
{
	struct vbus_watch *__w;

	list_for_each_entry(__w, __watches, list)
		if (!strcmp(__w->node, node))
			return __w;

	return NULL;
}

/*
 * Look for a precise watch in the list of watches. If the watch exists, it is returned (same pointer as the argument)
 * or NULL if it does not exist.
 */
static struct vbus_watch *get_watch(struct list_head *__watches, struct vbus_watch *w)
{
	struct vbus_watch *__w;

	list_for_each_entry(__w, __watches, list)
		if (!strcmp(__w->node, w->node) && (__w->callback == w->callback))
			return __w;

	return NULL;
}

void __register_vbus_watch(struct list_head *__watches, struct vbus_watch *watch) {
	unsigned long flags;

	/* A watch on a certain node with a certain callback has to be UNIQUE. */
	BUG_ON((get_watch(__watches, watch) != NULL));

	if (!find_first_watch(__watches, watch->node))
		vbs_watch(watch->node);
	else
		BUG();

	/* Prepare to update the list */

	local_irq_save(flags);

	watch->pending = false;

	list_add(&watch->list, __watches);

	local_irq_restore(flags);
}


/* Register a watch on a vbstore node */
void register_vbus_watch(struct vbus_watch *watch)
{
	if (smp_processor_id() == 1)
		__register_vbus_watch(&watches_rt, watch);
	else
		__register_vbus_watch(&watches, watch);


}
EXPORT_SYMBOL_GPL(register_vbus_watch);

static void ____unregister_vbus_watch(struct list_head *__watches, struct vbus_watch *watch, int vbus) {
	unsigned long flags;

	/* Prevent some undesired operations on the list */

	local_irq_save(flags);

	BUG_ON(!get_watch(__watches, watch));

	list_del(&watch->list);

	/* Check if we can remove the watch from vbstore if there is no associated watch
	 * (with the corresponding node name).
	 */
	if (vbus && !find_first_watch(__watches, watch->node)) {

		local_irq_restore(flags);

		vbs_unwatch(watch->node);

	} else
		local_irq_restore(flags);
}


static void __unregister_vbus_watch(struct vbus_watch *watch, int vbus)
{
	if (smp_processor_id() == 1)
		____unregister_vbus_watch(&watches_rt, watch, vbus);
	else
		____unregister_vbus_watch(&watches, watch, vbus);
}

void unregister_vbus_watch(struct vbus_watch *watch) {
	__unregister_vbus_watch(watch, 1);
}

void unregister_vbus_watch_without_vbus(struct vbus_watch *watch) {
	__unregister_vbus_watch(watch, 0);
}


/*
 * Main threaded function to monitor watches on vbstore entries. Various callbacks can be associated to an event path which is notified
 * by an event message. The pair <event message/callback> has to be unique.
 *
 * The following conditions are handled:
 *
 * IRQs are disabled during manipulations on event message and watch list. Interrupt routine could be executed during a callback operation and
 * some processed event messages may be removed.
 *
 * Secondly, a callback operation might decide to unregister some watches. The watch list has to remain consistent as well.
 *
 */
static int vbus_watch_thread(void *unused)
{
	struct vbus_watch *__w;
	bool found;

	BUG_ON(smp_processor_id() != AGENCY_CPU);

	for (;;) {

		wait_for_completion(&vbs_state.watch_wait);

		/* Avoiding to be suspended during SOO callback processing... */
		mutex_lock(&vbs_state.watch_mutex);

		do {
			found = false;

			list_for_each_entry(__w, &watches, list) {

				/* Is the watch is pending ? */
				if (__w->pending) {

					found = true;
					__w->pending = false;

					/* Execute the watch handler */
					__w->callback(__w);

					/* We go out of loop since a callback operation could manipulate the watch list */
					break;
				}
			}
		} while (found);

		mutex_unlock(&vbs_state.watch_mutex);
	}

	return 0;
}

irqreturn_t vbus_vbstore_isr(int irq, void *unused)
{
	vbus_msg_t *msg, *orig_msg, *orig_msg_tmp;
	struct vbus_watch *__w;
	bool found;

	/* Just make sure we are running on CPU #0 in RT configuration */
	BUG_ON(smp_processor_id() != AGENCY_CPU);

	DBG("IRQ: %d intf: %lx req_cons: %d  req_prod: %d\n", irq, __intf, __intf->req_cons, __intf->req_prod);
	DBG("IRQ: %d intf: %lx rsp_cons: %d  rsp_prod: %d\n", irq, __intf, __intf->rsp_cons, __intf->rsp_prod);

	BUG_ON(!hard_irqs_disabled());

	while (__intf->rsp_cons != __intf->rsp_prod) {

		msg = kzalloc(sizeof(vbus_msg_t), GFP_ATOMIC);
		if (msg == NULL)
			BUG();

		memset(msg, 0, sizeof(vbus_msg_t));

		/* Read the vbus_msg to process */
		vbs_read(msg, sizeof(vbus_msg_t));

		if (msg->len > 0) {
			msg->payload = kzalloc(msg->len, GFP_ATOMIC);
			if (msg->payload == NULL)
				BUG();

			memset(msg->payload, 0, msg->len);

			vbs_read(msg->payload, msg->len);

		} else
			/* Override previous value */
			msg->payload = NULL;

		if (msg->type == VBS_WATCH_EVENT) {

			found = false;

			list_for_each_entry(__w, &watches, list) {

				if (!strcmp(__w->node, msg->payload)) {
					__w->pending = true;
					found = true;
					mb();
				}
			}

			if (found)
				complete(&vbs_state.watch_wait);

			kfree(msg->payload);
			kfree(msg);


		} else {

			/* Look for the peer vbus_msg which did the request */
			found = false;

			list_for_each_entry_safe(orig_msg, orig_msg_tmp, &vbus_msg_standby_list, list) {
				if (orig_msg->id == msg->id) {
					list_del(&orig_msg->list); /* Remove this message from the standby list */

					found = true;
					orig_msg->reply = msg;
					mb();

					/* Wake up the thread waiting for the answer */
					complete(orig_msg->u.reply_wait);

					break; /* Ending the search loop */
				}
			}

			if (!found) /* A pending message MUST exist */
				BUG();

		}

	}

	BUG_ON(!hard_irqs_disabled());

	return IRQ_HANDLED;
}

static void vbs_dump_walk(char *nodename) {
	char **dir;
	unsigned int i, dir_n = 0;
	int ret;

	char _local[80];
	char val[20];
	char *msg = val;

	dir = vbus_directory(VBT_NIL, nodename, "", &dir_n);

	for (i = 0; i < dir_n; i++) {
		strcpy(_local, nodename);

		if (strcmp(_local, "/"))
			strcat(_local, "/");

		strcat(_local, dir[i]);

		ret = vbs_store_read(_local, &msg, 20);

		lprintk("%s ", _local);

		if (ret)
			printk("= %s", msg);

		lprintk("\n");

		vbs_dump_walk(_local);
	}

	kfree(dir);

}


void vbs_dump(void) {

	lprintk("----------- VBstore dump --------------\n");

	vbs_dump_walk("/");

	lprintk("--------------- end --------------------\n");
}

int vbus_vbstore_init(void)
{
	struct task_struct *task;
	struct vbus_device dev;
	int evtchn;
	struct sched_param param;
	int vbus_irq;

	/*
	 * Bind evtchn for interdomain communication: must be executed from the agency or from a ME.
	 */

	/* dev temporary used to set up event channel used by vbstore. */

	dev.otherend_id = 0;
	DBG("%s: binding a local event channel to the remote evtchn %d in Agency (intf: %lx) ...\n", __func__, __intf->revtchn, __intf);

	vbus_bind_evtchn(&dev, __intf->revtchn, &evtchn);

	/* This is our local event channel */
	__intf->levtchn = evtchn;

	DBG("Local vbstore_evtchn is %d (remote is %d)\n", __intf->levtchn, __intf->revtchn);

	INIT_LIST_HEAD(&vbs_state.reply_list);

	mutex_init(&vbs_state.request_mutex);
	mutex_init(&vbs_state.transaction_mutex);
	mutex_init(&vbs_state.watch_mutex);
	mutex_init(&vbs_state.transaction_group_mutex);

	spin_lock_init(&vbs_state.msg_list_lock);

	vbs_state.transaction_count = 0;

	init_completion(&vbs_state.watch_wait);

	/* Initialize the shared memory rings to talk to vbstore */
	vbus_irq = bind_evtchn_to_virq_handler(__intf->levtchn, vbus_vbstore_isr, NULL, IRQF_DISABLED, "vbus_vbstore", NULL);
	if (vbus_irq <= 0) {
		lprintk(KERN_ERR "VBus request irq failed %i\n", vbus_irq);
		BUG();
	}

	task = kthread_run(vbus_watch_thread, NULL, "vbswatch");
	if (IS_ERR(task))
		return PTR_ERR(task);

	param.sched_priority = 60;

	sched_setscheduler_nocheck(task, SCHED_FIFO, &param);

#ifndef CONFIG_X86
	vbus_probe_frontend_init();
#endif

	lprintk("%s: done.\n", __func__);

	return 0;
}
