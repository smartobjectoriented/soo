/*
 * Copyright (C) 2004 Philippe Gerum <rpm@xenomai.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/slab.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/heap.h>
#include <cobalt/kernel/registry.h>
#include <cobalt/kernel/thread.h>
#include <cobalt/kernel/apc.h>
#include <cobalt/kernel/assert.h>

/**
 * @ingroup cobalt_core
 * @defgroup cobalt_core_registry Registry services
 *
 * The registry provides a mean to index object descriptors on unique
 * alphanumeric keys. When labeled this way, an object is globally
 * exported; it can be searched for, and its descriptor returned to
 * the caller for further use; the latter operation is called a
 * "binding". When no object has been registered under the given name
 * yet, the registry can be asked to set up a rendez-vous, blocking
 * the caller until the object is eventually registered.
 *
 *@{
 */

struct xnobject *registry_obj_slots;
EXPORT_SYMBOL_GPL(registry_obj_slots);

static LIST_HEAD(free_object_list); /* Free objects. */

static LIST_HEAD(busy_object_list); /* Active and exported objects. */

static unsigned int nr_active_objects;

static unsigned long next_object_stamp;

static struct hlist_head *object_index;

static int nr_object_entries;

static struct xnsynch register_synch;

unsigned xnregistry_hash_size(void)
{
	static const int primes[] = {
		101, 211, 307, 401, 503, 601,
		701, 809, 907, 1009, 1103
	};

#define obj_hash_max(n)			 \
((n) < sizeof(primes) / sizeof(int) ? \
 (n) : sizeof(primes) / sizeof(int) - 1)

	return primes[obj_hash_max(CONFIG_XENO_OPT_REGISTRY_NRSLOTS / 100)];
}

int xnregistry_init(void)
{
	int n, ret __maybe_unused;

	registry_obj_slots = kmalloc(CONFIG_XENO_OPT_REGISTRY_NRSLOTS *
				     sizeof(struct xnobject), GFP_KERNEL);
	if (registry_obj_slots == NULL)
		return -ENOMEM;

	next_object_stamp = 0;

	for (n = 0; n < CONFIG_XENO_OPT_REGISTRY_NRSLOTS; n++) {
		registry_obj_slots[n].objaddr = NULL;
		list_add_tail(&registry_obj_slots[n].link, &free_object_list);
	}

	/* Slot #0 is reserved/invalid. */
	list_get_entry(&free_object_list, struct xnobject, link);
	nr_active_objects = 1;

	nr_object_entries = xnregistry_hash_size();
	object_index = kmalloc(sizeof(*object_index) *
				      nr_object_entries, GFP_KERNEL);

	if (object_index == NULL)
		return -ENOMEM;

	for (n = 0; n < nr_object_entries; n++)
		INIT_HLIST_HEAD(&object_index[n]);

	xnsynch_init(&register_synch, XNSYNCH_FIFO, NULL);

	return 0;
}

void xnregistry_cleanup(void)
{

	kfree(object_index);
	xnsynch_destroy(&register_synch);
	kfree(registry_obj_slots);
}


static unsigned registry_hash_crunch(const char *key)
{
	unsigned int h = 0, g;

#define HQON    24		/* Higher byte position */
#define HBYTE   0xf0000000	/* Higher nibble on */

	while (*key) {
		h = (h << 4) + *key++;
		if ((g = (h & HBYTE)) != 0)
			h = (h ^ (g >> HQON)) ^ g;
	}

	return h % nr_object_entries;
}

static inline int registry_hash_enter(const char *key, struct xnobject *object)
{
	struct xnobject *ecurr;
	unsigned s;

	object->key = key;
	s = registry_hash_crunch(key);

	hlist_for_each_entry(ecurr, &object_index[s], hlink)
		if (ecurr == object || strcmp(key, ecurr->key) == 0)
			return -EEXIST;

	hlist_add_head(&object->hlink, &object_index[s]);

	return 0;
}

static inline int registry_hash_remove(struct xnobject *object)
{
	unsigned int s = registry_hash_crunch(object->key);
	struct xnobject *ecurr;

	hlist_for_each_entry(ecurr, &object_index[s], hlink)
		if (ecurr == object) {
			hlist_del(&ecurr->hlink);
			return 0;
		}

	return -ESRCH;
}

static struct xnobject *registry_hash_find(const char *key)
{
	struct xnobject *ecurr;

	hlist_for_each_entry(ecurr, 
			&object_index[registry_hash_crunch(key)], hlink)
		if (strcmp(key, ecurr->key) == 0)
			return ecurr;

	return NULL;
}

struct registry_wait_context {
	struct xnthread_wait_context wc;
	const char *key;
};

static inline int registry_wakeup_sleepers(const char *key)
{
	struct registry_wait_context *rwc;
	struct xnthread_wait_context *wc;
	struct xnthread *sleeper, *tmp;
	int cnt = 0;

	xnsynch_for_each_sleeper_safe(sleeper, tmp, &register_synch) {
		wc = xnthread_get_wait_context(sleeper);
		rwc = container_of(wc, struct registry_wait_context, wc);
		if (*key == *rwc->key && strcmp(key, rwc->key) == 0) {
			xnsynch_wakeup_this_sleeper(&register_synch, sleeper);
			++cnt;
		}
	}

	return cnt;
}

/**
 * @fn int xnregistry_enter(const char *key,void *objaddr,xnhandle_t *phandle,struct xnpnode *pnode)
 * @brief Register a real-time object.
 *
 * This service allocates a new registry slot for an associated
 * object, and indexes it by an alphanumeric key for later retrieval.
 *
 * @param key A valid NULL-terminated string by which the object will
 * be indexed and later retrieved in the registry. Since it is assumed
 * that such key is stored into the registered object, it will *not*
 * be copied but only kept by reference in the registry. Pass an empty
 * or NULL string if the object shall only occupy a registry slot for
 * handle-based lookups.
 *
 * @param objaddr An opaque pointer to the object to index by @a
 * key.
 *
 * @param phandle A pointer to a generic handle defined by the
 * registry which will uniquely identify the indexed object, until the
 * latter is unregistered using the xnregistry_remove() service.
 *
 * @param pnode A pointer to an optional /proc node class
 * descriptor. This structure provides the information needed to
 * export all objects from the given class through the /proc
 * filesystem, under the /proc/xenomai/registry entry. Passing NULL
 * indicates that no /proc support is available for the newly
 * registered object.
 *
 * @return 0 is returned upon success. Otherwise:
 *
 * - -EINVAL is returned if @a objaddr is NULL, or if @a key is
 * non-NULL and contains an invalid '/' character.
 *
 * - -ENOMEM is returned if the system fails to get enough dynamic
 * memory from the global real-time heap in order to register the
 * object.
 *
 * - -EEXIST is returned if the @a key is already in use.
 *
 * @coretags{unrestricted, might-switch, atomic-entry}
 */
int xnregistry_enter(const char *key, void *objaddr,
		     xnhandle_t *phandle, struct xnpnode *pnode)
{
	struct xnobject *object;
	unsigned long s;
	int ret;

	if (objaddr == NULL || (key != NULL && strchr(key, '/')))
		return -EINVAL;

	xnlock_get_irqsave(&nklock, s);

	if (list_empty(&free_object_list)) {
		ret = -EAGAIN;
		goto unlock_and_exit;
	}

	object = list_get_entry(&free_object_list, struct xnobject, link);
	nr_active_objects++;
	object->objaddr = objaddr;
	object->cstamp = ++next_object_stamp;

	if (key == NULL || *key == '\0') {
		object->key = NULL;
		*phandle = object - registry_obj_slots;
		ret = 0;
		goto unlock_and_exit;
	}

	ret = registry_hash_enter(key, object);
	if (ret) {
		nr_active_objects--;
		list_add_tail(&object->link, &free_object_list);
		goto unlock_and_exit;
	}

	list_add_tail(&object->link, &busy_object_list);

	/*
	 * <!> Make sure the handle is written back before the
	 * rescheduling takes place.
	 */
	*phandle = object - registry_obj_slots;

	if (registry_wakeup_sleepers(key))
		xnsched_run();

unlock_and_exit:

	xnlock_put_irqrestore(&nklock, s);

	return ret;
}
EXPORT_SYMBOL_GPL(xnregistry_enter);

/**
 * @fn int xnregistry_bind(const char *key,xnticks_t timeout,int timeout_mode,xnhandle_t *phandle)
 * @brief Bind to a real-time object.
 *
 * This service retrieves the registry handle of a given object
 * identified by its key. Unless otherwise specified, this service
 * will block the caller if the object is not registered yet, waiting
 * for such registration to occur.
 *
 * @param key A valid NULL-terminated string which identifies the
 * object to bind to.
 *
 * @param timeout The timeout which may be used to limit the time the
 * thread wait for the object to be registered. This value is a wait
 * time given as a count of nanoseconds. It can either be relative,
 * absolute monotonic (XN_ABSOLUTE), or absolute adjustable
 * (XN_REALTIME) depending on @a timeout_mode. Passing XN_INFINITE @b
 * and setting @a timeout_mode to XN_RELATIVE specifies an unbounded
 * wait. Passing XN_NONBLOCK causes the service to return immediately
 * without waiting if the object is not registered on entry. All other
 * values are used as a wait limit.
 *
 * @param timeout_mode The mode of the @a timeout parameter. It can
 * either be set to XN_RELATIVE, XN_ABSOLUTE, or XN_REALTIME (see also
 * xntimer_start()).
 *
 * @param phandle A pointer to a memory location which will be written
 * upon success with the generic handle defined by the registry for
 * the retrieved object. Contents of this memory is undefined upon
 * failure.
 *
 * @return 0 is returned upon success. Otherwise:
 *
 * - -EINVAL is returned if @a key is NULL.
 *
 * - -EINTR is returned if xnthread_unblock() has been called for the
 * waiting thread before the retrieval has completed.
 *
 * - -EWOULDBLOCK is returned if @a timeout is equal to XN_NONBLOCK
 * and the searched object is not registered on entry. As a special
 * exception, this error is also returned if this service should
 * block, but was called from a context which cannot sleep
 * (e.g. interrupt, non-realtime or scheduler locked).
 *
 * - -ETIMEDOUT is returned if the object cannot be retrieved within
 * the specified amount of time.
 *
 * @coretags{primary-only, might-switch}
 *
 * @note xnregistry_bind() only returns the index portion of a handle,
 * which might include other fixed bits to be complete
 * (e.g. XNSYNCH_PSHARED). The caller is responsible for completing
 * the handle returned with those bits if applicable, depending on the
 * context.
 */
int xnregistry_bind(const char *key, xnticks_t timeout, int timeout_mode,
		    xnhandle_t *phandle)
{
	struct registry_wait_context rwc;
	struct xnobject *object;
	int ret = 0, info;
	unsigned long s;

	if (key == NULL)
		return -EINVAL;

	xnlock_get_irqsave(&nklock, s);

	if (timeout_mode == XN_RELATIVE &&
	    timeout != XN_INFINITE && timeout != XN_NONBLOCK) {
		timeout_mode = XN_REALTIME;
		timeout += xnclock_read();
	}

	for (;;) {
		object = registry_hash_find(key);
		if (object) {
			*phandle = object - registry_obj_slots;
			goto unlock_and_exit;
		}

		if ((timeout_mode == XN_RELATIVE && timeout == XN_NONBLOCK) ||
		    xnsched_unblockable_p()) {
			ret = -EWOULDBLOCK;
			goto unlock_and_exit;
		}

		rwc.key = key;
		xnthread_prepare_wait(&rwc.wc);
		info = xnsynch_sleep_on(&register_synch, timeout, timeout_mode);
		if (info & XNTIMEO) {
			ret = -ETIMEDOUT;
			goto unlock_and_exit;
		}
		if (info & XNBREAK) {
			ret = -EINTR;
			goto unlock_and_exit;
		}
	}

unlock_and_exit:

	xnlock_put_irqrestore(&nklock, s);

	return ret;
}
EXPORT_SYMBOL_GPL(xnregistry_bind);

/**
 * @fn int xnregistry_remove(xnhandle_t handle)
 * @brief Forcibly unregister a real-time object.
 *
 * This service forcibly removes an object from the registry. The
 * removal is performed regardless of the current object's locking
 * status.
 *
 * @param handle The generic handle of the object to remove.
 *
 * @return 0 is returned upon success. Otherwise:
 *
 * - -ESRCH is returned if @a handle does not reference a registered
 * object.
 *
 * @coretags{unrestricted}
 */
int xnregistry_remove(xnhandle_t handle)
{
	struct xnobject *object;
	int ret = 0;
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);

	object = xnregistry_validate(handle);
	if (object == NULL) {
		ret = -ESRCH;
		goto unlock_and_exit;
	}

	object->objaddr = NULL;
	object->cstamp = 0;

	if (object->key) {
		registry_hash_remove(object);

		list_del(&object->link);
	}

	list_add_tail(&object->link, &free_object_list);
	nr_active_objects--;

unlock_and_exit:

	xnlock_put_irqrestore(&nklock, s);

	return ret;
}
EXPORT_SYMBOL_GPL(xnregistry_remove);

/**
 * Turn a named object into an anonymous object
 *
 * @coretags{unrestricted}
 */
int xnregistry_unlink(const char *key)
{
	struct xnobject *object;
	int ret = 0;
	unsigned long s;

	if (key == NULL)
		return -EINVAL;

	xnlock_get_irqsave(&nklock, s);

	object = registry_hash_find(key);
	if (object == NULL) {
		ret = -ESRCH;
		goto unlock_and_exit;
	}
		
	ret = registry_hash_remove(object);
	if (ret < 0)
		goto unlock_and_exit;

#ifdef CONFIG_XENO_OPT_VFILE
	if (object->pnode) {
		registry_unexport_pnode(object);
		/*
		 * Leave the update of the object queues to
		 * the work callback if it has been kicked.
		 */
		if (object->pnode)
			goto unlock_and_exit;
	}
#endif /* CONFIG_XENO_OPT_VFILE */

	list_del(&object->link);
		
	object->key = NULL;

unlock_and_exit:
	xnlock_put_irqrestore(&nklock, s);

	return ret;
}

/**
 * @fn void *xnregistry_lookup(xnhandle_t handle, unsigned long *cstamp_r)
 * @brief Find a real-time object into the registry.
 *
 * This service retrieves an object from its handle into the registry
 * and returns the memory address of its descriptor. Optionally, it
 * also copies back the object's creation stamp which is unique across
 * object registration calls.
 *
 * @param handle The generic handle of the object to fetch.
 *
 * @param cstamp_r If not-NULL, the object's creation stamp will be
 * copied to this memory area.
 *
 * @return The memory address of the object's descriptor is returned
 * on success. Otherwise, NULL is returned if @a handle does not
 * reference a registered object.
 *
 * @coretags{unrestricted}
 */

/** @} */
