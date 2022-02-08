/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VUART_H
#define VUART_H

#include <soo/uapi/console.h>

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <soo/vdevback.h>
#include <soo/vdevfront.h>

#include <linux/vt_kern.h>

#define VUART_NAME	"vuart"
#define VUART_PREFIX	"[" VUART_NAME "] "

/*  This is a reserved char code we use to query (patched) Qemu to retrieve the window size. */
#define SERIAL_GWINSZ   '\254'

typedef struct {
	char str[CONSOLEIO_BUFFER_SIZE];
} vuart_request_t;

typedef struct {
	char dummy;
} vuart_response_t;

DEFINE_RING_TYPES(vuart, vuart_request_t, vuart_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vuart_back_ring_t ring;
	unsigned int irq;

} vuart_t;

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vuart_front_ring_t ring;

	rtdm_irq_t rtdm_irq;
	unsigned int irq;

	struct page *ring_pages;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vuartrt_t;

bool vuart_ready(void);
void vuart_send(char *str);

bool rt_log_enabled(void);
void rt_log_enable(void);
void rt_log_disable(void);

#endif /* VUART_H */
