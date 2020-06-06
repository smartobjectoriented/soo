/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef GUEST_API_INCLUDE
#define GUEST_API_INCLUDE

#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/device.h>

#include <soo/imec.h>

#ifdef DEBUG

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>

#undef DBG
#define DBG(fmt, ...) \
    do { \
        lprintk("(Dom#%d): %s:%i > "fmt, ME_domID(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define DBG0(...) DBG("%s", ##__VA_ARGS__)

#else
#define DBG(fmt, ...)
#define DBG0(...)
#endif


/*
 * Entry for a list of uevent which are propagated to the user space
 */
typedef struct {
	struct list_head list;
	unsigned int uevent_type;
	unsigned int slotID;
} soo_uevent_t;

int soo_uevent(struct device *dev, struct kobj_uevent_env *env);
int agency_ctl(agency_ctl_args_t *agency_ctl_args);


int get_ME_state(unsigned int ME_slotID);
int set_ME_state(unsigned int ME_slotID, ME_state_t state);

int get_agency_desc(agency_desc_t *SOO_desc);
void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc);
void get_ME_spid(unsigned int slotID, unsigned char *spid);

void vunmap_page_range(unsigned long addr, unsigned long end);

#endif /* GUEST_API_INCLUDE */

