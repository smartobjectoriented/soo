/*
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/plugin.h>

/**
 * Initialize the Transceiver functional block of Soolink.
 */
void transceiver_init(void) {

	lprintk("SOOlink: transceiver functional block initializing now ...\n");

	/* Internal blocks initialization */
	sender_init();
	receiver_init();

	/* Initialize the SOOlink plugins (registered as SOOlink initcall). */
	transceiver_plugin_init();
}
