
/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <soo/core/sysfs.h>

#include <soo/uapi/console.h>

/* Required to have a real string for each attribute. */
char *soo_sysfs_names[] = {
	[buffer_count] = "buffer_count",
	[neighbours] = "neighbours",
};

static struct kobject *root_kobj;
static struct kobject *soolink_kobj;
static struct kobject *soolink_discovery_kobj;

/* Internal list structure to manage the diversity of callback functions related to attributes */
typedef struct {
	soo_sysfs_attr_t attr;
	sysfs_handler_t show_handler, store_handler;
	struct list_head list;
} handler_list_t;

static struct list_head handlers;

void soo_sysfs_register(soo_sysfs_attr_t attr, sysfs_handler_t show_handler, sysfs_handler_t store_handler) {
	handler_list_t *handler;

	handler = kmalloc(sizeof(handler_list_t), GFP_ATOMIC);
	BUG_ON(!handler);

	handler->attr = attr;
	handler->show_handler = show_handler;
	handler->store_handler = store_handler;

	list_add(&handler->list, &handlers);
}


ssize_t	attr_show(struct kobject *kobj, struct kobj_attribute *attr, char *str) {
	struct list_head *cur;
	handler_list_t *handler;

	list_for_each(cur, &handlers) {
		handler = list_entry(cur, handler_list_t, list);

		if (!strcmp(attr->attr.name, soo_sysfs_names[handler->attr])) {
			if (handler->show_handler) {
				handler->show_handler(str);

				return strlen(str);
			}
		}
	}

	return 0;
}


ssize_t	attr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *str, size_t len) {
	struct list_head *cur;
	handler_list_t *handler;

	list_for_each(cur, &handlers) {
		handler = list_entry(cur, handler_list_t, list);

		if (!strcmp(attr->attr.name, soo_sysfs_names[handler->attr])) {
			if (handler->store_handler)
				handler->store_handler((char *) str);
		}
	}

	return len;
}

/***** Attributes declaration *****/

/* SOO */

/** SOOlink **/

/**** Discovery ****/

static struct kobj_attribute buffer_count_attr = __ATTR(buffer_count, 0664, attr_show, attr_store);
static struct kobj_attribute neighbours_attr = __ATTR(neighbours, 0664, attr_show, attr_store);

/* Group of attributes for SOOlink Discovery */
static struct attribute *soolink_discovery_attrs[] = {
	&buffer_count_attr.attr,
	&neighbours_attr.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group soolink_discovery_group = {
	.attrs = soolink_discovery_attrs,
};

void soo_sysfs_init(void) {
        int ret;

        INIT_LIST_HEAD(&handlers);
        
        /* Create the entry in /sys to access the information
         * which will be exposed to the user space.
         */

	/* SOO root */
        root_kobj = kobject_create_and_add("soo", NULL);
	BUG_ON(!root_kobj);

	/** SOOlink **/
	soolink_kobj = kobject_create_and_add("soolink", root_kobj);
	BUG_ON(!soolink_kobj);

	/*** Discovery ***/
	soolink_discovery_kobj = kobject_create_and_add("discovery", soolink_kobj);
	BUG_ON(!soolink_discovery_kobj);

	ret = sysfs_create_group(soolink_discovery_kobj, &soolink_discovery_group);
	BUG_ON(ret);

}


