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

#include <stdarg.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <soo/uapi/console.h>

#include <soo/core/asf.h>

#if 0
#define DEBUG
#endif

/* ASF supported commands */
#define ASF_TA_CMD_ENCODE		0
#define ASF_TA_CMD_DECODE		1
#define ASF_TA_CMD_KEY_SZ		2

#define ASF_MAX_BUFF_SIZE		(1 << 19)
#define ASF_TAG_SIZE			16
#define ASF_IV_SIZE				12

static bool asf_enabled = false;

/* UUID of ASF Trusted application */
static uint8_t asf_uuid[] = { 0x6a, 0xca, 0x96, 0xec, 0xd0, 0xa4, 0x11, 0xe9,
			                  0xbb, 0x65, 0x2a, 0x2a, 0xe2, 0xdb, 0xcc, 0xe4};

static unsigned asf_key_size = 0;
static size_t asf_shm_size;

/************************************************************************
 *                          ASF core                                    *
 ************************************************************************/

/**
 * asf_tee_ctx_match() - check TEE device version
 *
 *  Callback called by tee_client_open_context()
 */
static int asf_tee_ctx_match(struct tee_ioctl_version_data *ver, const void *data)
{
	if (ver->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
	else
		return 0;
}

/**
 * asf_open_session() - Open a context & session with ASF TEE
 *
 * @ctx: TEE context
 * @return the TEE session ID or -1 in case of error
 */
static int asf_open_session(struct tee_context **ctx)
{
	struct tee_ioctl_open_session_arg sess_arg;
	int ret;

	/* Initialize a context connecting us to the TEE */
	*ctx = tee_client_open_context(NULL, asf_tee_ctx_match, NULL, NULL);
	if (IS_ERR(*ctx)) {
		lprintk("ASF ERROR - Open Context failed\n");
		return -1;
	}

	memset(&sess_arg, 0, sizeof(sess_arg));

	memcpy(sess_arg.uuid, asf_uuid, TEE_IOCTL_UUID_LEN);
	sess_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	sess_arg.num_params = 0;

	ret = tee_client_open_session(*ctx, &sess_arg, NULL);
	if ((ret < 0) || (sess_arg.ret != 0)) {
		lprintk("ASF ERROR - Open Session failed, error: %x\n", sess_arg.ret);
		return -1;
	}

	return sess_arg.session;
}

/**
 * asf_close_session() - Close a context & session ASF TEE opened by asf_open_session
 *
 * @ctx: TEE context
 * @session_id: the TEE session ID
 * @return 0 in case of success or -1
 */
static int asf_close_session(struct tee_context *ctx, int session_id)
{
	int ret;

	ret = tee_client_close_session(ctx, session_id);
	if (ret)  {
		lprintk("ASF ERROR - Close Context failed\n");
		return -1;
	}

	tee_client_close_context(ctx);

	return 0;
}

static struct tee_shm *asf_shm_alloc(struct tee_context *ctx)
{
	struct tee_shm *shm = NULL;
	size_t shm_sz = 2*ASF_MAX_BUFF_SIZE + ASF_TAG_SIZE + ASF_IV_SIZE;

#warning check without TEE_SHM_DMA_BUF option
	shm = tee_shm_alloc(ctx, shm_sz, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(shm)) {
		lprintk("ASF ERROR - share buffer allocation failed\n");
		return NULL;
	}

	return shm;
}

static void asf_shm_free(struct tee_shm *shm)
{
	tee_shm_free(shm);

	/* The share buffer are freed only after one jiffies */
	msleep(1000 / HZ);
	flush_scheduled_work();
}


/**
 * asf_invoke_cypto() - Call the TEE crypto command ()
 *
 * @ctx: TEE context
 * @session_id: the TEE session ID
 * @mode: action mode (ASF_TA_CMD_ENCODE or ASF_TA_CMD_DECODE)
 * @bufin: Input buffer
 * @bufout: output/result buffer
 * @buf_sz: input/output buffer size
 * @iv: IV used for the encoding (output) or IV to use for the decoding (input)
 * @tag: Result tag (encoding) - tag to use for the decoding
 * @return 0 in case of success or -1
 */
static int asf_invoke_cypto(struct tee_context *ctx, int session_id, int mode, struct tee_shm *shm,
		           uint8_t *bufin, uint8_t *bufout, size_t buf_sz, uint8_t *iv, uint8_t *tag)
{
	struct tee_ioctl_invoke_arg arg;
	uint8_t *data_in = NULL;
	uint8_t *data_out = NULL;
	uint8_t *data_tag = NULL;
	uint8_t *data_iv = NULL;
	struct tee_param param[1];
	int ret = 0;

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));
	arg.func = mode;
	arg.session = session_id;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = shm;
	param[0].u.memref.shm_offs = 0;
	param[0].u.memref.size = buf_sz;

	data_in  = tee_shm_get_va(shm, 0);
	data_out = tee_shm_get_va(shm, buf_sz);
	data_iv  = tee_shm_get_va(shm, 2*buf_sz);
	data_tag = tee_shm_get_va(shm, 2*buf_sz + ASF_IV_SIZE);

	memcpy(data_in, bufin, buf_sz);
	if (mode == ASF_TA_CMD_DECODE) {
		memcpy(data_tag, tag, ASF_TAG_SIZE);
		memcpy(data_iv, iv, ASF_IV_SIZE);
	}

	ret = tee_client_invoke_func(ctx, &arg, param);
	if ((ret) || (arg.ret)) {
		lprintk("ASF ERROR - cryptographic action, in asf PTA, failed\n");
		return -1;
	}

	memcpy(bufout, data_out, buf_sz);
	if (mode == ASF_TA_CMD_ENCODE) {
		memcpy(iv, data_iv, ASF_IV_SIZE);
		memcpy(tag, data_tag, ASF_TAG_SIZE);
	}

	return ret;
}

/**
 * asf_res_buf_alloc() - Allocation of the result buffer
 *
 * @slice_nr: The number of slices composing the input buffer
 * @rest: the size of the last slice (non-full slice)
 * @mode: action mode (ASF_TA_CMD_ENCODE or ASF_TA_CMD_DECODE)
 * @res_buf: result buffer
 * @return the size of the result buffer or -1 in case of error
 */
static size_t asf_res_buf_alloc(size_t slice_nr, size_t rest, int mode, uint8_t **res_buf)
{
	size_t rest_sz;
	size_t slice_sz;
	size_t res_buf_sz;

	if (mode == ASF_TA_CMD_ENCODE) {
		/* Encoding case (add IV and Tag to result) */
		slice_sz = ASF_MAX_BUFF_SIZE + ASF_TAG_SIZE + ASF_IV_SIZE;
		rest_sz  = rest + ASF_TAG_SIZE + ASF_IV_SIZE;
	} else {
		/* decoding case (remove IV and Tag to result) */
		slice_sz = ASF_MAX_BUFF_SIZE;
		rest_sz  = rest - ASF_TAG_SIZE - ASF_IV_SIZE;
	}

	res_buf_sz  = slice_nr * slice_sz;
	res_buf_sz += rest_sz;

	*res_buf = (uint8_t *)kmalloc(res_buf_sz, GFP_KERNEL);
	if (!*res_buf) {
		lprintk("ASF ERROR - Buffer allocation failed\n");
		return -1;
	}

	return res_buf_sz;
}

/**
 * asf_send_slice_buffer() - Slice a buffer and send the blocks to TEE
 *
 * @ctx: TEE context
 * @session_id: TEE Session ID
 * @mode: TEE action mode (ASF_TA_CMD_ENCODE or ASF_TA_CMD_DECODE)
 * @inbuf: Input buffer for the TEE action
 * @outbuf: Result buffer with the result of the TEE action
 * @slice_nr: The number of slices composing the input buffer
 * @rest: the size of the last slice (non-full slice)
 * @return 0 in case of success or -1 in case of error
 */
static int asf_send_slice_buffer(struct tee_context *ctx, uint32_t session_id, int mode, uint8_t *inbuf, uint8_t *outbuf,  size_t slice_nr, size_t rest)
{
	unsigned loop_nr;
	struct tee_shm *shm;
	unsigned i;
	size_t rest_sz;
	size_t cur_buf_sz;
	uint8_t *in = inbuf;
	uint8_t *out = outbuf;
	uint8_t *tag;
	uint8_t *iv;
	int ret;

	/* Allocation of shared buffer */
	shm = asf_shm_alloc(ctx);
	if (!shm)
		return -1;

	if (mode == ASF_TA_CMD_ENCODE)
		rest_sz = rest;
	else
		rest_sz = rest - ASF_TAG_SIZE - ASF_IV_SIZE;

	loop_nr = slice_nr;
	if (rest)
		loop_nr++;

	for (i = 0; i < loop_nr; i++) {
		if (i == (loop_nr-1)) {
			/* last loop, update its size */
			if (rest)
				cur_buf_sz = rest_sz;
			else
				cur_buf_sz = ASF_MAX_BUFF_SIZE;
		} else {
			cur_buf_sz = ASF_MAX_BUFF_SIZE;
		}

		/* Compute position of elements of the current encoded block */
		if (mode == ASF_TA_CMD_ENCODE)
			iv  = out + cur_buf_sz;
		else
			iv = in + cur_buf_sz;

		tag = iv + ASF_IV_SIZE;

		ret = asf_invoke_cypto(ctx, session_id, mode, shm, in, out, cur_buf_sz, iv, tag);
		if (ret) {
			lprintk("ASF ERROR - Buffer decryption failed\n");
			return -1;
		}

		/* Update buffer position */
		if (mode == ASF_TA_CMD_ENCODE) {
			in  += cur_buf_sz;
			out += cur_buf_sz + ASF_IV_SIZE + ASF_TAG_SIZE;
		} else {
			in  += cur_buf_sz + ASF_IV_SIZE + ASF_TAG_SIZE;
			out += cur_buf_sz;
		}
	}

	asf_shm_free(shm);

	return 0;
}

/**
 * asf_get_key_size() -  Get the size of the key used
 *
 * return the key size or -1 in case of error
 */
static int asf_get_key_size(void)
{
	struct tee_ioctl_invoke_arg arg;
	struct tee_param param[1];
	struct tee_context *ctx;
	uint32_t session_id;
	int ret;
	int key_sz;

	/* Initialize a context connecting us to the TEE */
	session_id = asf_open_session(&ctx);
	if (session_id < 0)
		return -1;

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));
	arg.func = ASF_TA_CMD_KEY_SZ;
	arg.session = session_id;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	ret = tee_client_invoke_func(ctx, &arg, param);
	key_sz = param[0].u.value.a;

	ret = asf_close_session(ctx, session_id);
	if (ret)
		return ret;

	return key_sz;
}


/************************************************************************
 *                             APIs                                     *
 ************************************************************************/


int asf_encrypt(sym_key_t key, uint8_t *plain_buf, size_t plain_buf_sz, uint8_t **enc_buf)
{
	struct tee_context *ctx;
	uint32_t session_id;
	int res_buf_sz;
	int ret;
	int block_nr;
	int rest;

	/* 1. Encoded/result buffer allocation */
	rest     = (plain_buf_sz % ASF_MAX_BUFF_SIZE);
	block_nr = (plain_buf_sz / ASF_MAX_BUFF_SIZE);

	res_buf_sz = asf_res_buf_alloc(block_nr, rest, ASF_TA_CMD_ENCODE, enc_buf);
	if (res_buf_sz < 0)
		return -1;

	/* 2. Initialize a context connecting us to the TEE */
	session_id = asf_open_session(&ctx);
	if (session_id < 0) {
		goto err_encode;
	}

	/* 3. 'Slicing' the buffer and sent it to TEE */
	ret = asf_send_slice_buffer(ctx, session_id, ASF_TA_CMD_ENCODE, plain_buf, *enc_buf, block_nr, rest);
	if (ret)
		goto err_encode;

	/* 4. close context */
	ret = asf_close_session(ctx, session_id);
	if (ret)
		goto err_encode;

	return res_buf_sz;

err_encode:
	kfree(*enc_buf);

	return -1;
}

int asf_decrypt(sym_key_t key, uint8_t *enc_buf, size_t enc_buf_sz, uint8_t **plain_buf)
{
	struct tee_context *ctx;
	uint32_t session_id;
	int ret;
	size_t res_buf_sz;
	uint8_t *res_buf = NULL;
	int block_nr;
	int rest;

	/* 1. Plain/result buffer allocation */
	rest     = (enc_buf_sz % (ASF_MAX_BUFF_SIZE + ASF_IV_SIZE + ASF_TAG_SIZE));
	block_nr = (enc_buf_sz / (ASF_MAX_BUFF_SIZE + ASF_IV_SIZE + ASF_TAG_SIZE));

	res_buf_sz = asf_res_buf_alloc(block_nr, rest, ASF_TA_CMD_DECODE, &res_buf);
	if (res_buf_sz < 0)
		return -1;

	/* 2. Initialize a context connecting us to the TEE */
	session_id = asf_open_session(&ctx);
	if (session_id < 0) {
		goto err_decode;
	}

	/* 3. 'Slicing' the buffer in bloc of ASF_MAX_BUFF_SIZE size */
	ret = asf_send_slice_buffer(ctx, session_id, ASF_TA_CMD_DECODE, enc_buf, res_buf, block_nr, rest);

	/* 4. close context */
	ret = asf_close_session(ctx, session_id);

	*plain_buf = res_buf;

	return res_buf_sz;

err_decode:

	kfree(res_buf);

	return -1;
}


/************************************************************************
 *                     ASF test and examples                            *
 ************************************************************************/

void asf_example(void)
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

void asf_big_buf(void)
{
	int size;
	int i;
	uint8_t *plain;
	uint8_t *encoded;
	uint8_t *decoded;
	int plain_sz = 1046628;

	lprintk(" ===  Testing big buffer ===\n");
	lprintk(" 	   buffer size: %d\n", plain_sz);

	plain = (uint8_t *)kmalloc(plain_sz, GFP_KERNEL);
	if (!plain) {
		lprintk("\n!!!!! ======== BIG BUFFER ALLOCATION FAILED ======== !!!!!\n\n");
		return;
	}

	for (i = 0; i < plain_sz; i++)
		plain[i] = (uint8_t)i;

	lprintk(" ===  Testing big buffer (encoding) ===\n");

	size = asf_encrypt(ASF_KEY_COM, plain, plain_sz, &encoded);
	lprintk("Encoded buffer size: %d\n", size);

	if (size < 0)
		return;

	lprintk(" ===  Testing big buffer (decoding) ===\n");

	size = asf_decrypt(ASF_KEY_COM, encoded, size, &decoded);
	lprintk("decoded buffer size: %d\n", size);

	if (size != plain_sz)
		lprintk("\n!!!!! ======== Result size is wrong, got %d, expected %d\n", size, plain_sz);

	for (i = 0; i < size; i++) {
		if (decoded[i] != (uint8_t)i)
			lprintk(" WRONG decoded DATA\n");
	}

	kfree(plain);
	kfree(encoded);
	kfree(decoded);
}

/************************************************************************
 *                     ASF Initialization                               *
 ************************************************************************/

static int asf_init(void)
{
	/* Check if TrustZone is activated. Currently, this is done
	 * by examining the presence of psci property in the device tree.
	 */
	if (!of_find_node_by_name(NULL, "psci"))
		return 0;

	lprintk("Agency Security Framework initialization...\n");

	/* Get the size of the key. It is needed for buffer padding */


	asf_key_size = asf_get_key_size();
	asf_shm_size = tee_get_shm_size();

#if 0
	asf_big_buf();
#endif
	asf_example();

	asf_enabled = true;

	return 0;
}

/* be sure tee driver is loaded before loading asf */
late_initcall(asf_init);
