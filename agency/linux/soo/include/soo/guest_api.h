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
        lprintk("(Dom#%d): %s:%i > "fmt, smp_processor_id(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define DBG0(...) DBG("%s", ##__VA_ARGS__)

#else
#define DBG(fmt, ...)
#define DBG0(...)
#endif

int get_agency_desc(agency_desc_t *SOO_desc);

void vunmap_page_range(unsigned long addr, unsigned long end);

#endif /* GUEST_API_INCLUDE */

