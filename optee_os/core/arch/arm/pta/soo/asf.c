// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2014-2019 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
 */

#include <string.h>
#include <kernel/pseudo_ta.h>

#include <crypto/crypto.h>

#define ASF_TA_NAME     "asf.ta"

#define ASF_UUID \
		{ 0x6aca96ec, 0xd0a4, 0x11e9, \
			{ 0xbb, 0x65, 0x2a, 0x2a, 0xe2, 0xdb, 0xcc, 0xe4 }}


#define ASF_TAG_SIZE		16
#define ASF_IV_SZ			12

/* Supported commands */
#define AFS_TA_CMD_ENCODE		0
#define AFS_TA_CMD_DECODE		1

static uint8_t aes_key[] =  {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
	0x3b, 0x4e, 0x12, 0x61, 0x82, 0xea, 0x2d, 0x6a,
	0xac, 0x7f, 0x15, 0x88, 0x90, 0xfc, 0xf4, 0xc3
};


static uint8_t asf_iv[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x0,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0
};


static void asf_iv_inc(void)
{
	int i;

	for (i=0; i<ASF_IV_SZ; i++) {
		asf_iv[i]++;

		if (asf_iv[i] == 0)
			/* Result = 0, so increment next "digit" */
			continue;
		else
			/* Increment completed */
			break;
	}
}


static TEE_Result asf_enc_dec(uint32_t type, TEE_Param params[TEE_NUM_PARAMS], TEE_OperationMode mode)
{
	uint8_t *inbuf;
	uint8_t *outbuf;
	size_t buf_sz;
	uint8_t *tag;
	uint8_t *iv;
	size_t tag_sz = ASF_TAG_SIZE;
	int ret;
	void *ctx = NULL;

	/* Checking parameters */
	if (TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
			            TEE_PARAM_TYPE_NONE,
			            TEE_PARAM_TYPE_NONE,
			            TEE_PARAM_TYPE_NONE) != type) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	buf_sz = params[0].memref.size;

	inbuf  = params[0].memref.buffer;
	outbuf = inbuf + buf_sz;
	tag    = inbuf + 2*buf_sz + ASF_IV_SZ;

	if (mode == TEE_MODE_ENCRYPT)
		iv = asf_iv;
	else
		iv = inbuf + 2*buf_sz;

	ret = crypto_authenc_alloc_ctx(&ctx, TEE_ALG_AES_GCM);
	if (ret != TEE_SUCCESS) {
		EMSG("ASF - crypto_cipher_alloc_ctx failed, ret: %u\n", ret);
		return ret;
	}

	ret = crypto_authenc_init(ctx, TEE_ALG_AES_GCM, mode,
			       aes_key, sizeof(aes_key),
			       iv, ASF_IV_SZ,
			       ASF_TAG_SIZE,
		    	   0, 0);		/* Not used parameters */
	if (ret != TEE_SUCCESS) {
		EMSG("ASF - crypto_cipher_init failed, ret: 0x%x\n", ret);
		return ret;
	}

	if (mode == TEE_MODE_ENCRYPT) {
		ret = crypto_authenc_enc_final(ctx, TEE_ALG_AES_GCM, inbuf, buf_sz, outbuf, &buf_sz, tag, &tag_sz);
		if (ret != TEE_SUCCESS) {
			EMSG("ASF - crypto_authenc_enc_final failed, ret: 0x%x\n", ret);
			return ret;
		}

		/* Copy current IV and increment it for next use */
		memcpy(inbuf + 2*buf_sz, iv, ASF_IV_SZ);
		asf_iv_inc();

	} else if (mode == TEE_MODE_DECRYPT) {

		ret =  crypto_authenc_dec_final(ctx, TEE_ALG_AES_GCM, inbuf, buf_sz, outbuf, &buf_sz, tag, ASF_TAG_SIZE);
		if (ret != TEE_SUCCESS) {
			EMSG("ASF - crypto_authenc_dec_final failed, ret: 0x%x\n", ret);
			/* Decryption failed --> reset the output buffer */
			memset(outbuf, 0, buf_sz);
			return ret;
		}

	} else {
		EMSG("ASF - Encryption/decryption mode 0x%x is not supported", mode);
	}

	crypto_authenc_final(ctx, TEE_ALG_AES_GCM);
	crypto_authenc_free_ctx(ctx, TEE_ALG_AES_GCM);

	return TEE_SUCCESS;
}

static TEE_Result asf_encode_buf(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	return asf_enc_dec(type, params, TEE_MODE_ENCRYPT);
}

static TEE_Result asf_decode_buf(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	return asf_enc_dec(type, params, TEE_MODE_DECRYPT);
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result asf_invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case AFS_TA_CMD_ENCODE:
		return asf_encode_buf(ptypes, params);
	case AFS_TA_CMD_DECODE:
		return asf_decode_buf(ptypes, params);
	default:
		EMSG("ASF - Command  Not Supported (%d)", cmd);
		break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = ASF_UUID, .name = ASF_TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = asf_invoke_command);
