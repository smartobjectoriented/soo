/*
 * Copyright (C) 2004 Philippe Gerum <rpm@xenomai.org>
 *
 * Xenomai is free software; you can redistribute it and/or
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
#ifndef _COBALT_KERNEL_REGISTRY_H
#define _COBALT_KERNEL_REGISTRY_H

#include <cobalt/kernel/list.h>
#include <cobalt/kernel/synch.h>

/**
 * @addtogroup cobalt_core_registry
 *
 * @{
 */
struct xnpnode;

struct xnobject {
	void *objaddr;
	const char *key;	  /* !< Hash key. May be NULL if anonynous. */
	unsigned long cstamp;		  /* !< Creation stamp. */

	struct hlist_node hlink; /* !< Link in h-table */
	struct list_head link;
};

int xnregistry_init(void);

void xnregistry_cleanup(void);



#define DEFINE_XNPTREE(__var, __name);

/* Placeholders. */

struct xnpnode {
	const char *dirname;
};

struct xnpnode_snapshot {
	struct xnpnode node;
};

struct xnpnode_regular {
	struct xnpnode node;
};

struct xnpnode_link {
	struct xnpnode node;
};

/* Public interface. */

extern struct xnobject *registry_obj_slots;

static inline struct xnobject *xnregistry_validate(xnhandle_t handle)
{
	struct xnobject *object;
	/*
	 * Careful: a removed object which is still in flight to be
	 * unexported carries a NULL objaddr, so we have to check this
	 * as well.
	 */
	handle = xnhandle_get_index(handle);
	if (likely(handle && handle < CONFIG_XENO_OPT_REGISTRY_NRSLOTS)) {
		object = &registry_obj_slots[handle];
		return object->objaddr ? object : NULL;
	}

	return NULL;
}

static inline const char *xnregistry_key(xnhandle_t handle)
{
	struct xnobject *object = xnregistry_validate(handle);
	return object ? object->key : NULL;
}

int xnregistry_enter(const char *key,
		     void *objaddr,
		     xnhandle_t *phandle,
		     struct xnpnode *pnode);

static inline int
xnregistry_enter_anon(void *objaddr, xnhandle_t *phandle)
{
	return xnregistry_enter(NULL, objaddr, phandle, NULL);
}

int xnregistry_bind(const char *key,
		    xnticks_t timeout,
		    int timeout_mode,
		    xnhandle_t *phandle);

int xnregistry_remove(xnhandle_t handle);

static inline
void *xnregistry_lookup(xnhandle_t handle,
			unsigned long *cstamp_r)
{
	struct xnobject *object = xnregistry_validate(handle);

	if (object == NULL)
		return NULL;

	if (cstamp_r)
		*cstamp_r = object->cstamp;

	return object->objaddr;
}

int xnregistry_unlink(const char *key);

unsigned xnregistry_hash_size(void);

extern struct xnpnode_ops xnregistry_vfsnap_ops;

extern struct xnpnode_ops xnregistry_vlink_ops;

/** @} */

#endif /* !_COBALT_KERNEL_REGISTRY_H */
