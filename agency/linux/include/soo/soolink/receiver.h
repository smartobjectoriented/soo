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

#ifndef RECEIVER_H
#define RECEIVER_H

#include <linux/types.h>

#include <soo/soolink/transceiver.h>
#include <soo/soolink/plugin.h>

void __receiver_rx(sl_desc_t *sl_desc, void *packet, size_t size);
void receiver_rx(sl_desc_t *sl_desc, transceiver_packet_t *packet);

void receiver_init(void);

#endif /* RECEIVER_H */

