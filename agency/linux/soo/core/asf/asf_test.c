/*
 * Copyright (C) 2019-2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include "asf_priv.h"


#define TA_HELLO_WORLD_CMD_INC_VALUE		0


static uint8_t hello_world_uuid[] = { 0x8a, 0xaa, 0xf2, 0x00, 0x24, 0x50, 0x11, 0xe4,
		                              0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b};


/**
 *  asf_crypto_example() - example of ASF encryption/decryption flow
 */
void asf_crypto_example(void)
{
	int size;
	uint8_t  plain[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	                    0x08, 0x09, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6};
	uint8_t *encoded = NULL;
	uint8_t *decoded = NULL;

	lprintk("## %s: Encoding\n", __func__);
	size = asf_encrypt(ASF_KEY_COM, plain, sizeof(plain), &encoded);
	if (size > 0) {
		lprintk("## %s: Encoded buffer size: %d\n", __func__, size);
		lprintk("buffer: ");
		lprintk_buffer(encoded, size);
	} else {
		lprintk("## %s: Encoding failed: %d\n", __func__);
		return;
	}

	lprintk("## %s: Decoding\n", __func__);
	size = asf_decrypt(ASF_KEY_COM, encoded, size, &decoded);
	if (size > 0) {
		lprintk("## %s: decoded buffer size: %d\n", __func__, size);
		lprintk("buffer: ");
		lprintk_buffer(decoded, size);
	} else {
		lprintk("## %s: Decoding failed: %d\n", __func__);
		kfree(encoded);
		return;
	}

	kfree(encoded);
	kfree(decoded);
}

/**
 * hello_world_ta_cmd() - Send 'Inc value' cmd to Hello World TA
 */
int hello_world_ta_cmd(hello_args_t *args)
{
	struct tee_context *ctx;
	uint32_t session_id;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param param[1];
	int ret1, ret2;

	/* Open Session */
	session_id = asf_open_session(&ctx, hello_world_uuid);
	if (session_id < 0) {
		printk("ASF Error - Open session failed\n");
		return -1;
	}

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));
	arg.func = TA_HELLO_WORLD_CMD_INC_VALUE;
	arg.session = session_id;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param[0].u.value.a = args->val;

	ret1 = tee_client_invoke_func(ctx, &arg, param);
	if ((ret1) || (arg.ret)) {
		lprintk("ASF ERROR - Installation of TA failed\n");
		ret1 = 1;
	} else {
		printk("ASF - hello_world_cmd result: %d\n", (int)param[0].u.value.a);
	}

	ret2 = asf_close_session(ctx, session_id);
	if (ret2)
		lprintk("ASF ERROR - Close session failed\n");

	if ((ret1) || (ret2))
		return -1;
	else
		return 0;

}

