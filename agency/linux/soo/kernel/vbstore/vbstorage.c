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

#include <linux/kernel.h>
#include <linux/slab.h> /* for kmalloc */
#include <linux/gfp.h> /* GFP Flags for kmalloc */
#include <linux/string.h>
#include <linux/of.h>

#include <soo/uapi/console.h>
#include <soo/vbstore.h>

/* The vbs store tree */
static struct vbs_node *vbs_root;

/* Allocate a new node, with the given key / value. 
 * returns the node or NULL. */
static struct vbs_node *vbs_store_new_node(const char *key, const char *value) {
	struct vbs_node *new;
	char *lkey, *lvalue = NULL;

	if (value != NULL) {
		lvalue = kstrdup(value, GFP_ATOMIC);
		if (!lvalue)
			BUG();
	}

	lkey = kstrdup(key, GFP_ATOMIC);

	if (!lkey)
		BUG();

	new = kmalloc(sizeof(struct vbs_node), GFP_ATOMIC);

	if (new) {
		new->key = lkey;
		new->value = lvalue;
		new->parent = NULL;
		INIT_LIST_HEAD(&new->children);
		INIT_LIST_HEAD(&new->watchers);
	} else
		BUG();


	return new;
}

/*
 * Get the absolute path of a specific vbs node.
 */
void vbs_get_absolute_path(struct vbs_node *node, char *path) {
	char __path[VBS_KEY_LENGTH];

	strcpy(path, node->key);

	/* shortcut */
	if (node->parent == NULL)
		return ;

	/* The root node key is equal to "" */
	while (strcmp(node->parent->key, "")) {

		/* New level in the ascending path */
		strcpy(__path, node->parent->key);
		strcat(__path, "/");
		strcat(__path, path);

		/* Then replace */
		strcpy(path, __path);

		node = node->parent;
	}
}

struct vbs_node *vbs_store_lookup(const char *key) {
	struct vbs_node *parent_node, *node;
	char *left, *free_ptr, *path_elem;
	bool found;

	/* free_ptr is used to preserve the reference to the string since left will be updated by strsep() */
	free_ptr = left = kstrdup(key, GFP_ATOMIC);
	if (free_ptr == NULL)
		/* ENOMEM -> lookup failed. */
		return NULL;

	/* Skip initial slash */
	if (left[0] == '/')
		left++;

	/* Search query for SLASH. */
	if (strlen(left) == 0) {
		/* ENOMEM -> lookup failed. */
		kfree(free_ptr);
		return vbs_root;
	}

	parent_node = vbs_root;

	/* Find next path_elem to be searched for. */
	while ((path_elem = strsep(&left, "/")) != NULL) {
		found = false;

		list_for_each_entry(node, &parent_node->children, sibling)
		{
			if (strcmp(node->key, path_elem) == 0)
			{
				found = true;
				parent_node = node;
				break;
			}
		}
		if (!found) {
			/* path_elem not found in the children -> lookup failed. */
			kfree(free_ptr);
			return NULL;
		}
	}

	kfree(free_ptr);
	return parent_node;
}

/**
 * Updates value with the content associated to key.
 * Returns 1 on success
 *         0 on error, value is unmodified. */
int vbs_store_read(const char *key, char **value, size_t size) {
	struct vbs_node *node;

	DBG("VBS_STORE_READ(%s)", key);

	node = vbs_store_lookup(key);
	if (!node)
	{
		DBG("%s: not available (yet?)\n", key);
		return 0;
	}

	if (!node->value)
		return 0;

	if (strlen(node->value) >= size)
		return 0;

	strcpy(*value, node->value);

	DBG("value = %s\n", *value);
	return 1;
}

/** Wrapper for those of us which do not want notifications. */
int vbs_store_write(const char *key, const char *value)
{
	struct vbs_node *node;
	int ret;

	ret = vbs_store_write_notify(key, value, &node);

	return ret;
}

/**
 * Create or update the value associated to a key.
 * */
int vbs_store_write_notify(const char *key, const char *value, struct vbs_node **notify_node) {
	struct vbs_node *parent, *new;
	int base_length;
	char *parent_key, *lvalue = NULL;

	DBG("VBS_STORE_WRITE(%s) = %s\n", key, value);

	new = vbs_store_lookup(key);
	if (new) {
		/* We are lucky, this is an update of an existing node. */
		kfree(new->value);
		if (value) {
			lvalue = kstrdup(value, GFP_ATOMIC);
			if (!lvalue)
				BUG();
		}

		new->value = lvalue;
		*notify_node = new;

		return 0;
	}

	/* We have to find the parent node. */
	base_length = (unsigned int) (strrchr(key, '/') - key);

	parent_key = kmalloc(base_length + 1, GFP_ATOMIC);
	if (!parent_key)
		BUG();

	strncpy(parent_key, key, base_length);
	parent_key[base_length] = '\0';

	parent = vbs_store_lookup(parent_key);
	if (!parent) {
		lprintk("Cannot find parent key: %s\n", parent_key);
		BUG();
	}

	/* Create a new child and set it up. */
	new = vbs_store_new_node((char *) (key + base_length + 1), value);
	if (!new)
		BUG();

	/* Attach the new sibling to its parent. */
	list_add_tail(&new->sibling, &parent->children);
	new->parent = parent;

	*notify_node = parent;

	return 0;
}

int vbs_store_mkdir(const char *key) {

	lprintk("%s: creating backend entry for %s virtual driver...\n", __func__, key);

	return vbs_store_write(key, NULL);
}

/*
 * Remove a watcher from the watcher list.
 */
static void del_watchers(struct list_head *watchers) {
	struct vbs_watcher *tmp;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, watchers) {
		tmp = list_entry(pos, struct vbs_watcher, list);

		list_del(pos);

		kfree(tmp);
	}
}

/* Recursive remove of children */
static void del_children(struct list_head *sibling) {
	struct vbs_node *tmp;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, sibling) {
		tmp = list_entry(pos, struct vbs_node, sibling);

		DBG("removing %s...\n", tmp->key);

		if (!list_empty(&tmp->children))
			del_children(&tmp->children);

		if (!list_empty(&tmp->watchers))
			del_watchers(&tmp->watchers);

		list_del(pos);

		kfree(tmp->key);
		kfree(tmp->value);
		kfree(tmp);
	}

}

/* Remove a specific node */
int vbs_store_rm(const char *key) {
	struct vbs_node *node;

	DBG("removing %s...\n", key);
	node = vbs_store_lookup(key);
	if (!node) {
		printk(" %s: entry %s not existing...\n", __func__, key);
		return 0;
	}

	/* Remove all children */
	if (!list_empty(&node->children))
		del_children(&node->children);

	list_del(&node->sibling);

	kfree(node->key);
	kfree(node->value);
	kfree(node);

	return 0;
}


/**
 * Set children to the children names.
 * Return the written length or 0 on error.
 */
int vbs_store_readdir(const char *key, char *children, const size_t size_children) {
	struct vbs_node *node, *parent;
	int tot_len = 0;
	char *string = children;

	parent = vbs_store_lookup(key);
	if (!parent)
		return 0;

	list_for_each_entry(node, &parent->children, sibling)
	{
		int len = strlen(node->key);
		tot_len += len + 1;
		if (tot_len >= size_children)
			return 0;

		strcpy(string, node->key);
		string[len] = '\0';
		string += len + 1;
	}

	return tot_len;
}


/**
 * '1' is written into the first byte of result if the directory exists, '0' otherwise.
 */
int vbs_store_dir_exists(const char *key, char *result) {
	struct vbs_node *parent;
	result[1] = '\0';

	parent = vbs_store_lookup(key);
	if (!parent)
		result[0] = '0';
	else
		result[0] = '1';

	return 1;
}

int vbs_store_watch(const char *key, vbstore_intf_t *intf) {
	struct vbs_node *node;
	struct vbs_watcher *watcher;

	DBG("VBS_STORE_WATCH(%s) on corresponding levtchn(%i) intf = %p\n", key, intf->levtchn, intf);

	watcher = kmalloc(sizeof(struct vbs_watcher), GFP_ATOMIC);
	if (!watcher)
		goto no_mem0;

	INIT_LIST_HEAD(&watcher->list);

	node = vbs_store_lookup(key);
	if (!node)
		goto not_found;

	watcher->intf = intf;

	list_add_tail(&watcher->list, &node->watchers);

	return 0;

not_found:

	kfree(watcher);
	lprintk(" %s: key %s not_found\n", __func__, key);
	BUG();

no_mem0:
	lprintk(" %s: no memory\n", __func__);
	BUG();
}

int vbs_store_unwatch(const char *key, vbstore_intf_t *intf) {
	struct vbs_node *node;
	struct vbs_watcher *watcher;
	struct list_head *p, *next;

	DBG("removing watch: %s on intf = %p\n", key, intf);

	node = vbs_store_lookup(key);
	if (!node) {
		printk(" %s: entry %s not existing...\n", __func__, key);
		return 0;
	}

	list_for_each_safe(p, next, &node->watchers) {
		watcher = list_entry(p, struct vbs_watcher, list);
		if (watcher->intf == intf) {
			list_del(p);
			kfree(watcher);
			return 0;
		}
	}

	return 0;
}

void vbstorage_agency_init(void) {
	struct device_node *np;

	vbus_probe_backend_init();

	/* Initialize Root node. */
	vbs_root = vbs_store_new_node("", NULL);
	if (!vbs_root)
		panic("VBstore: ERROR: Could not allocate VBstore root node\n");

	/* Root keys, which are common to every drivers. */
	vbs_store_mkdir("/backend");
	vbs_store_mkdir("/device");

#warning the following entries (domain + directcomm) need to be revisited (see soo/me entries)

	vbs_store_mkdir("/domain");
	vbs_store_mkdir("/domain/gnttab");
	vbs_store_mkdir("/domain/gnttab/0");
	vbs_store_mkdir("/domain/gnttab/1");
	vbs_store_mkdir("/domain/gnttab/2");
	vbs_store_mkdir("/domain/gnttab/3");
	vbs_store_mkdir("/domain/gnttab/4");
	vbs_store_mkdir("/domain/gnttab/5");
	vbs_store_mkdir("/domain/gnttab/6");

	/* Target ME directcomm event channel */
	vbs_store_mkdir("/soo");
	vbs_store_mkdir("/soo/directcomm");
	vbs_store_mkdir("/soo/directcomm/0");
	vbs_store_mkdir("/soo/directcomm/1");
	vbs_store_mkdir("/soo/directcomm/2");
	vbs_store_mkdir("/soo/directcomm/3");
	vbs_store_mkdir("/soo/directcomm/4");
	vbs_store_mkdir("/soo/directcomm/5");
	vbs_store_mkdir("/soo/directcomm/6");

	/* Prepare the entries for ME ID information */
	vbs_store_mkdir("/soo/me");

	/* Agency backend side of virtual LED device  */
	np = of_find_compatible_node(NULL, NULL, "vleds,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vleds");

	/* Agency backend side of virtual UI handler device */
	np = of_find_compatible_node(NULL, NULL, "vuihandler,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vuihandler");

	/* Agency backend side of virtual UART device */
	np = of_find_compatible_node(NULL, NULL, "vuart,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vuart");

	/* Agency backend side of virtual dummy device (reference backend) */
	np = of_find_compatible_node(NULL, NULL, "vdummy,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vdummy");

	/* Agency backend side of virtual weather (weather station) */
	np = of_find_compatible_node(NULL, NULL, "vweather,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vweather");

	/* Agency backend side of virtual DOGA 12V 6NM device */
	np = of_find_compatible_node(NULL, NULL, "vdoga12v6nm,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vdoga12v6nm");

	/* Agency backend side of virtual senseled (Sense HAT led) device */
	np = of_find_compatible_node(NULL, NULL, "vsenseled,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vsenseled");

	/* Agency backend side of virtual sensej (Sense HAT joystick) device */
	np = of_find_compatible_node(NULL, NULL, "vsensej,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vsensej");

	/* Agency backend side of virtual wagoled device */
	np = of_find_compatible_node(NULL, NULL, "vwagoled,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/vwagoled");

	/* Agency backend side of virtual en0cean device */
	np = of_find_compatible_node(NULL, NULL, "venocean,backend");
	if (of_device_is_available(np))
		vbs_store_mkdir("/backend/venocean");
}
