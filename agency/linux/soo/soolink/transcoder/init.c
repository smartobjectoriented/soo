/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <soo/soolink/coder.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/discovery.h>

/**
 * Initialize the Transcoder functional block of Soolink.
 */
void transcoder_init(void) {
	coder_init();
	decoder_init();
}

/*
 * Initiate a netstream with neighbours.
 */
void transcoder_stream_init(sl_desc_t *sl_desc) {
	/* Ask the transcoder to deactivate the Discovery */
	discovery_disable();

	/* Initiate the netstream. Now or soon, we will become speaker. In the meanwhile, we will
	 * be designated as listener.
	 */
	sender_request_xmit(sl_desc);
}

void transcoder_stream_terminate(sl_desc_t *sl_desc) {
	sender_request_xmit(sl_desc);
}
