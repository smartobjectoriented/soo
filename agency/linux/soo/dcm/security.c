/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include <linux/slab.h>

#include <soo/uapi/console.h>

#include <soo/core/asf.h>
#include <soo/dcm/security.h>

size_t security_encrypt(void *plain_buf, size_t plain_buf_sz, void **enc_buf)
{
	return asf_encrypt(ASF_KEY_COM, (uint8_t *)plain_buf, plain_buf_sz, (uint8_t **)enc_buf);
}

size_t security_decrypt(void *enc_buf, size_t enc_buf_sz, void **plain_buf)
{
	return asf_decrypt(ASF_KEY_COM, (uint8_t *)enc_buf, enc_buf_sz, (uint8_t **)plain_buf);
}

#if 0

/* DEBUG */
void dcm_asf_test(void)
{
	int size;
	uint8_t  plain[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	                    0x08, 0x09, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6};
	uint8_t *encoded = NULL;
	uint8_t *decoded = NULL;

	lprintk("## %s: Example - Encoding\n", __func__);
	size = asf_encrypt(ASF_KEY_COM, plain, sizeof(plain), &encoded);
	lprintk("Encoded buffer size: %d\n", size);
	if (size > 0)
		lprintk_buffer(encoded, size);

	lprintk("## %s: Example - Decoding\n", __func__);
	size = asf_decrypt(ASF_KEY_COM, encoded, size, &decoded);
	lprintk("decoded buffer size: %d\n", size);

	lprintk("buffer: ");
	if (size > 0)
		lprintk_buffer(decoded, size);

	kfree(encoded);
	kfree(decoded);
}
/* DEBUG */



#endif
