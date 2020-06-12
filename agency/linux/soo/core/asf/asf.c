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
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/file.h>

#include <soo/uapi/asf.h>

#include "asf_priv.h"

#if 0
#define DEBUG
#endif

/* ASF TA supported commands */
#define ASF_TA_CMD_ENCODE		0
#define ASF_TA_CMD_DECODE		1

#define ASF_MAX_BUFF_SIZE		(1 << 19)
#define ASF_TAG_SIZE			16
#define ASF_IV_SIZE				12

/* Management pTA boostrap command. It is used to install TA in Secure Storage */
#define PTA_SECSTOR_TA_MGMT_BOOTSTRAP	0

static struct tee_context *asf_ctx;
static int asf_sess_id;

static bool asf_enabled = false;
static bool asf_ctx_openend = false;

/* UUID of ASF Trusted application */
static uint8_t asf_uuid[] = { 0x6a, 0xca, 0x96, 0xec, 0xd0, 0xa4, 0x11, 0xe9,
			                  0xbb, 0x65, 0x2a, 0x2a, 0xe2, 0xdb, 0xcc, 0xe4};

/* UUID for management pTA. It is responsible to install TA in Security Storage */
static uint8_t mgmt_ta_uuid[] = { 0x6e, 0x25, 0x6c, 0xba, 0xfc, 0x4d, 0x49, 0x41,
				                  0xad, 0x09, 0x2c, 0xa1, 0x86, 0x03, 0x42, 0xdd };


DEFINE_MUTEX(asf_mutex);

/************************************************************************
 *                          ASF core                                    *
 ************************************************************************/

/**
 * asf_open_session() - Open a context & session with ASF TEE
 *
 * @ctx: TEE context
 * @return the TEE session ID or -1 in case of error
 */
int asf_open_session(struct tee_context **ctx, uint8_t *uuid)
{
	struct tee_ioctl_open_session_arg sess_arg;
	int ret;

	/* Initialize a context connecting us to the TEE */
	*ctx = teedev_open(optee_svc->teedev);
	if (IS_ERR(*ctx)) {
		lprintk("ASF ERROR - Open Context failed\n");
		return -1;
	}

	memset(&sess_arg, 0, sizeof(sess_arg));

	memcpy(sess_arg.uuid, uuid, TEE_IOCTL_UUID_LEN);
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
int asf_close_session(struct tee_context *ctx, int session_id)
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
	size_t shm_sz = ASF_MAX_BUFF_SIZE + ASF_TAG_SIZE + ASF_IV_SIZE;

	shm = tee_shm_alloc(ctx, shm_sz, TEE_SHM_MAPPED);
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
#if 0 /* Current tee driver does not delay the 'release' of the shm */
	msleep(jiffies_to_msecs(1));
	flush_scheduled_work();
#endif
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
		           sym_key_t key, uint8_t *bufin, uint8_t *bufout, size_t buf_sz, uint8_t *iv, uint8_t *tag)
{
	struct tee_ioctl_invoke_arg arg;
	uint8_t *data_inout = NULL;
	uint8_t *data_tag = NULL;
	uint8_t *data_iv = NULL;
	struct tee_param param[2];
	int ret = 0;

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));
	arg.func = mode;
	arg.session = session_id;
	arg.num_params = 2;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = shm;
	param[0].u.memref.shm_offs = 0;
	param[0].u.memref.size =  buf_sz + ASF_TAG_SIZE + ASF_IV_SIZE ;

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = key;

	data_inout  = tee_shm_get_va(shm, 0);
	data_iv  = tee_shm_get_va(shm, buf_sz);
	data_tag = tee_shm_get_va(shm, buf_sz + ASF_IV_SIZE);

	memcpy(data_inout, bufin, buf_sz);
	if (mode == ASF_TA_CMD_DECODE) {
		memcpy(data_tag, tag, ASF_TAG_SIZE);
		memcpy(data_iv, iv, ASF_IV_SIZE);
	}

	ret = tee_client_invoke_func(ctx, &arg, param);
	if ((ret) || (arg.ret)) {
		lprintk("ASF ERROR - cryptographic action, in asf PTA, failed with ret = %d\n", ret);
		return -1;
	}

	memcpy(bufout, data_inout, buf_sz);
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
static ssize_t asf_res_buf_alloc(size_t slice_nr, size_t rest, int mode, uint8_t **res_buf)
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

	*res_buf = (uint8_t *)kmalloc(res_buf_sz, GFP_ATOMIC);
	if (!*res_buf) {
		lprintk("ASF ERROR - Buffer allocation failed\n");
		return -1;
	}

	return (ssize_t)res_buf_sz;
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
static int asf_send_slice_buffer(struct tee_context *ctx, uint32_t session_id, int mode, sym_key_t key, uint8_t *inbuf, uint8_t *outbuf,  size_t slice_nr, size_t rest)
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

		ret = asf_invoke_cypto(ctx, session_id, mode, shm, key, in, out, cur_buf_sz, iv, tag);
		if (ret) {
			lprintk("ASF ERROR - asf_invoke_cypto failed with ret = %d\n", ret);
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

/************************************************************************
 *                             APIs                                     *
 ************************************************************************/

ssize_t asf_encrypt(sym_key_t key, uint8_t *plain_buf, size_t plain_buf_sz, uint8_t **enc_buf)
{
	ssize_t res_buf_sz;
	int ret;
	int block_nr;
	int rest;

	mutex_lock(&asf_mutex);

	/* 1. Encoded/result buffer allocation */
	rest     = (plain_buf_sz % ASF_MAX_BUFF_SIZE);
	block_nr = (plain_buf_sz / ASF_MAX_BUFF_SIZE);

	res_buf_sz = asf_res_buf_alloc(block_nr, rest, ASF_TA_CMD_ENCODE, enc_buf);
	if (res_buf_sz < 0)
		return -1;

	/* 3. 'Slicing' the buffer and sent it to TEE */
	ret = asf_send_slice_buffer(asf_ctx, asf_sess_id, ASF_TA_CMD_ENCODE, key, plain_buf, *enc_buf, block_nr, rest);
	if (ret)
		goto err_encode;

	mutex_unlock(&asf_mutex);

	return res_buf_sz;

err_encode:
	mutex_unlock(&asf_mutex);
	kfree(*enc_buf);

	return -1;
}

ssize_t asf_decrypt(sym_key_t key, uint8_t *enc_buf, size_t enc_buf_sz, uint8_t **plain_buf)
{
	int ret;
	ssize_t res_buf_sz;
	uint8_t *res_buf = NULL;
	int block_nr;
	int rest;

	mutex_lock(&asf_mutex);

	/* 1. Plain/result buffer allocation */
	rest     = (enc_buf_sz % (ASF_MAX_BUFF_SIZE + ASF_IV_SIZE + ASF_TAG_SIZE));
	block_nr = (enc_buf_sz / (ASF_MAX_BUFF_SIZE + ASF_IV_SIZE + ASF_TAG_SIZE));

	res_buf_sz = asf_res_buf_alloc(block_nr, rest, ASF_TA_CMD_DECODE, &res_buf);
	if (res_buf_sz < 0)
		return -1;

	/* 3. 'Slicing' the buffer in block of ASF_MAX_BUFF_SIZE size */
	ret = asf_send_slice_buffer(asf_ctx, asf_sess_id, ASF_TA_CMD_DECODE, key, enc_buf, res_buf, block_nr, rest);

	*plain_buf = res_buf;

	mutex_unlock(&asf_mutex);

	return res_buf_sz;

	return -1;
}

/************************************************************************
 *                     Char drv related operations                      *
 ************************************************************************/

ssize_t asf_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	uint32_t session_id;
	uint8_t *data_in = NULL;
	struct tee_context *ctx;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param param[1];
	struct tee_shm *shm = NULL;
	int ret1, ret2;

	printk("ASF - Write --> TA Installation\n");

	/* Open Session */
	session_id = asf_open_session(&ctx, mgmt_ta_uuid);
	if (session_id < 0) {
		printk("ASF Error - Open session failed\n");
		return -1;
	}

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));

	arg.func = PTA_SECSTOR_TA_MGMT_BOOTSTRAP;
	arg.session = session_id;
	arg.num_params = 1;

	shm = tee_shm_alloc(ctx, len, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	data_in  = tee_shm_get_va(shm, 0);
	memcpy(data_in, buf, len);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm;
	param[0].u.memref.shm_offs = 0;
	param[0].u.memref.size = len;

	ret1 = tee_client_invoke_func(ctx, &arg, param);
	if ((ret1) || (arg.ret)) {
		lprintk("ASF ERROR - Installation of TA failed\n");
		ret1 = 1;
	}

	ret2 = asf_close_session(ctx, session_id);
	if (ret2)
		lprintk("ASF ERROR - Close session failed\n");

	if ((ret1) || (ret2))
		return -1;
	else
		return len;
}

long asf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int res;

	switch (cmd) {

	case ASF_IOCTL_CRYPTO_TEST:
		asf_crypto_example();
		asf_crypto_large_buf_test();
		return 0;

	case ASF_IOCTL_HELLO_WORLD_TEST:
		return hello_world_ta_cmd((hello_args_t *)(arg));

	case ASF_IOCTL_OPEN_SESSION:
		asf_sess_id = asf_open_session(&asf_ctx, asf_uuid);
		if (asf_sess_id < 0) {
			return -1;
		} else {
			asf_ctx_openend = true;
			return 0;
		}

	case ASF_IOCTL_SESSION_OPENED:
		return asf_ctx_openend;

	case ASF_IOCTL_CLOSE_SESSION:
		res = asf_close_session(asf_ctx, asf_sess_id);
		if (res == 0)
			asf_ctx_openend = false;
		return res;

	default:
		lprintk("ASF ioctl %d unavailable !\n", cmd);
		panic("ASF");
	}

	return -EINVAL;
}

struct file_operations asf_fops = {
		.owner = THIS_MODULE,
		.unlocked_ioctl = asf_ioctl,
		.write = asf_write,
};

/************************************************************************
 *                     ASF Initialization                               *
 ************************************************************************/

static int asf_init(void)
{
	int rc;

	/* Check if TrustZone is activated. Currently, this is done
	 * by examining the presence of psci property in the device tree.
	 */
	if (!of_find_node_by_name(NULL, "psci"))
		return 0;

	lprintk("Agency Security Framework initialization...\n");

	/* Registering device */
	rc = register_chrdev(ASF_DEV_MAJOR, ASF_DEV_NAME, &asf_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", ASF_DEV_MAJOR);
		BUG();
	}

	asf_enabled = true;

	return 0;
}

/* be sure tee driver is loaded before loading asf */
late_initcall(asf_init);
