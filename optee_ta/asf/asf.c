// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2014-2019 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <string.h>
#include <asf_ta.h>

#define ASF_KEY_SIZE		32 /* AES 256 */
#define ASF_TAG_SIZE		16
#define ASF_IV_SZ		12

typedef enum {
	ASF_KEY_COM = 0, /* Symmetric key for the 'Communication' flow */
	ASF_KEY_INJECT,  /* Symmetric key for the 'Injection' flow */
} sym_key_t;

static uint8_t aes_com_key[ASF_KEY_SIZE];
static uint8_t aes_inject_key[ASF_KEY_SIZE];



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
	uint8_t *buf;
	uint32_t buf_sz;
	uint8_t *tag;
	uint8_t *iv;
	uint8_t *key = NULL;
	uint32_t tag_sz = ASF_TAG_SIZE;
	int ret = TEE_SUCCESS;
	TEE_Attribute attr;
	TEE_OperationHandle op_handle = TEE_HANDLE_NULL;
	TEE_ObjectHandle key_handle = TEE_HANDLE_NULL;

	/* Checking parameters */
	if (TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
			            TEE_PARAM_TYPE_VALUE_INPUT,
			            TEE_PARAM_TYPE_NONE,
			            TEE_PARAM_TYPE_NONE) != type) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	buf_sz = params[0].memref.size;
	buf_sz = (buf_sz - ASF_TAG_SIZE - ASF_IV_SZ);

	buf = params[0].memref.buffer;
	tag = buf + buf_sz + ASF_IV_SZ;

	if (mode == TEE_MODE_ENCRYPT)
		iv = asf_iv;
	else
		iv = buf + buf_sz;

	/* get the key to use */
	if (params[1].value.a == ASF_KEY_COM) {
		key = aes_com_key;
	} else if (params[1].value.a == ASF_KEY_INJECT) {
		key = aes_inject_key;
	} else {
		EMSG("ASF - Unsupported key (%d)\n", params[1].value.a);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	ret = TEE_AllocateOperation(&op_handle, TEE_ALG_AES_GCM, mode, ASF_KEY_SIZE * 8);
	if (ret != TEE_SUCCESS) {
		EMSG("ASF - TEE_AllocateOperation failed, ret: %u\n", ret);
		return ret;
	}

	/* Set key - the key is store in the session handle */
	ret = TEE_AllocateTransientObject(TEE_TYPE_AES, ASF_KEY_SIZE * 8, &key_handle);
	if (ret != TEE_SUCCESS) {
		EMSG("ASF -Failed to allocate transient object");
		key_handle = TEE_HANDLE_NULL;
		goto out_free_op;
	}

	TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key, ASF_KEY_SIZE);

	ret = TEE_PopulateTransientObject(key_handle, &attr, 1);
	if (ret != TEE_SUCCESS) {
		EMSG("TEE_PopulateTransientObject failed, %x", ret);
		goto out_free_key;
	}

	ret = TEE_SetOperationKey(op_handle, key_handle);

	/* Crypto operation */

	ret = TEE_AEInit(op_handle, iv, ASF_IV_SZ, ASF_TAG_SIZE*8, 0, 0);
	if (ret != TEE_SUCCESS) {
		EMSG("ASF - crypto_cipher_init failed, ret: 0x%x\n", ret);
		goto out_free_key;
	}

	if (mode == TEE_MODE_ENCRYPT) {

		ret = TEE_AEEncryptFinal(op_handle, buf, buf_sz, buf, &buf_sz, tag, &tag_sz);
		if (ret != TEE_SUCCESS) {
			EMSG("ASF - crypto_authenc_enc_final failed, ret: 0x%x\n", ret);
			goto out_free_key;
		}

		/* Copy current IV and increment it for next use */
		memcpy(buf + buf_sz, iv, ASF_IV_SZ);
		asf_iv_inc();

	} else if (mode == TEE_MODE_DECRYPT) {

		ret = TEE_AEDecryptFinal(op_handle, buf, buf_sz, buf, &buf_sz, tag, ASF_TAG_SIZE);
		if (ret != TEE_SUCCESS) {
			EMSG("ASF - crypto_authenc_dec_final failed, ret: 0x%x\n", ret);
			/* Decryption failed --> reset the output buffer */
			memset(buf, 0, buf_sz);
			goto out_free_key;
		}

	} else {
		EMSG("ASF - Encryption/decryption mode 0x%x is not supported", mode);
	}

out_free_key:
	TEE_FreeTransientObject(key_handle);
out_free_op:
	TEE_FreeOperation(op_handle);

	return ret;
}

static TEE_Result asf_encode_buf(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	return asf_enc_dec(type, params, TEE_MODE_ENCRYPT);
}

static TEE_Result asf_decode_buf(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	return asf_enc_dec(type, params, TEE_MODE_DECRYPT);
}

TEE_Result TA_CreateEntryPoint(void)
{
	/* Nothing to do */

	return TEE_SUCCESS;
}


void TA_DestroyEntryPoint(void)
{
	/* Nothing to do */
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
	uint32_t com_key_size;
	uint32_t inject_key_size;
	int res;

	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;
	(void)&sess_ctx;

	/* Get AES communication key */

	/* The max len of the buffer must be specified and is checked by the function */
	com_key_size = ASF_KEY_SIZE;

	res = TEE_GetPropertyAsBinaryBlock(TEE_PROPSET_CURRENT_TA, "gp.ta.com_key", aes_com_key, &com_key_size);
	if ((res != TEE_SUCCESS) || (com_key_size != ASF_KEY_SIZE)) {
		EMSG("ASF - Recuperation of asf communication key failed with res = %x\n", res);
		return TEE_ERROR_GENERIC;
	}

	/* Get AES injector key */

	/* The max len of the buffer must be specified and is checked by the function */
	inject_key_size = ASF_KEY_SIZE;

	res = TEE_GetPropertyAsBinaryBlock(TEE_PROPSET_CURRENT_TA, "gp.ta.inject_key", aes_inject_key, &inject_key_size);
	if ((res != TEE_SUCCESS) || (inject_key_size != ASF_KEY_SIZE)) {
		EMSG("ASF - Recuperation of asf communication key failed");
		return TEE_ERROR_GENERIC;
	}

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx; /* Unused parameter */
}


TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	(void)&sess_ctx; /* Unused parameter */

	switch (cmd_id) {
	case AFS_TA_CMD_ENCODE:
		return asf_encode_buf(param_types, params);
	case AFS_TA_CMD_DECODE:
		return asf_decode_buf(param_types, params);
	default:
		EMSG("ASF - Command  Not Supported (%d)", cmd_id);
		break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}
