/*
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/vmalloc.h>
#include <linux/errno.h>

#include <asm/string.h>

#include <soo/uapi/dcm.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#if defined(CONFIG_DCM_LZ4)
#include <linux/lz4.h>
#endif /* CONFIG_DCM_LZ4 */

static compressor_method_t *compressor_methods[COMPRESSOR_N_METHODS] = { NULL };

#if defined(CONFIG_DCM_LZ4)
static uint8_t *work_lz4_mem;
#endif /* CONFIG_DCM_LZ4 */

static int no_compression_compress_data(void **data_compressed, void *source_data, size_t source_size) {
	compressor_data_t *alloc_data = vmalloc(source_size + sizeof(compressor_data_t));

	memcpy(alloc_data->payload, (void *) source_data, source_size);

	alloc_data->compression_method = COMPRESSOR_NO_COMPRESSION;
	alloc_data->decompressed_size = source_size + sizeof(compressor_data_t);
	*data_compressed = alloc_data;

	return alloc_data->decompressed_size;
}

static int no_compression_decompress_data(void **data_decompressed, compressor_data_t *data, size_t compressed_size) {
	size_t size_decompressed = data->decompressed_size;
	void *alloc_buffer = vmalloc(size_decompressed);

	memcpy(alloc_buffer, data->payload, size_decompressed);

	*data_decompressed = alloc_buffer;

	return size_decompressed;
}

/*
 * Callbacks of the "no compression" method
 */
static compressor_method_t method_no_compression = {
	.compress_callback 	= no_compression_compress_data,
	.decompress_callback	= no_compression_decompress_data
};

#if defined(CONFIG_DCM_LZ4)

static int lz4_compress_data(void **data_compressed, void *source_data, size_t source_size) {

	/* lz4_compressbound gives the worst reachable size after compression */
	compressor_data_t *alloc_data;
	size_t size_compressed;
	int ret;

	alloc_data = vmalloc(LZ4_compressBound(source_size) + sizeof(compressor_data_t));

	if ((size_compressed = LZ4_compress_default((const char *) source_data, alloc_data->payload, source_size, LZ4_compressBound(source_size), work_lz4_mem)) < 0) {
		lprintk("Error when compressing the ME\n");
		return ret;
	}

	alloc_data->compression_method = COMPRESSOR_LZ4;
	alloc_data->decompressed_size = source_size;

	/* Add the size of the compressor_data_t header */
	size_compressed += sizeof(compressor_data_t);

	*data_compressed = alloc_data;

	DBG("Original size: %d, compressed size (+header): %d\n", source_size, size_compressed);

	return size_compressed;
}

static int lz4_decompress_data(void **data_decompressed, compressor_data_t *data, size_t compressed_size) {

	/* The decompressed buffer as received by this function has a special header managed
	 * by the compressor in order to retrieve the size.
	 */
	size_t decompressed_size = data->decompressed_size;
	void *alloc_buffer;

	alloc_buffer = __vmalloc(decompressed_size, GFP_HIGHUSER | __GFP_ZERO, PAGE_SHARED | PAGE_KERNEL);
	BUG_ON(alloc_buffer == NULL);

	if (LZ4_decompress_fast(data->payload, alloc_buffer, decompressed_size) < 0) {
		lprintk("Error when decompressing the ME\n");
		BUG();
	}
	*data_decompressed = alloc_buffer;

	DBG("(Guessed) compressed size: %d, decompressed size: %d\n", compressed_size, decompressed_size);

	return decompressed_size;
}

/*
 * Callbacks of the LZ4 method
 */
static compressor_method_t method_lz4 = {
	.compress_callback 	= lz4_compress_data,
	.decompress_callback	= lz4_decompress_data
};

#endif /* CONFIG_DCM_LZ4 */

int compress_data(uint8_t method, void **data_compressed, void *source_data, size_t source_size) {
	if ((compressor_methods[method]) && (compressor_methods[method]->compress_callback))
		return compressor_methods[method]->compress_callback(data_compressed, source_data, source_size);

	return -EINVAL;
}

int decompress_data(void **data_decompressed, void *data_compressed, size_t compressed_size) {
	compressor_data_t *data = (compressor_data_t *) data_compressed;
	uint8_t method = data->compression_method;

	if ((compressor_methods[method]) && (compressor_methods[method]->decompress_callback))
		return compressor_methods[method]->decompress_callback(data_decompressed, data, compressed_size);

	return -EINVAL;
}

void compressor_method_register(uint8_t method, compressor_method_t *method_desc) {
	compressor_methods[method] = method_desc;
}

void compressor_init(void) {
	compressor_method_register(COMPRESSOR_NO_COMPRESSION, &method_no_compression);

#if defined(CONFIG_DCM_LZ4)
	work_lz4_mem = vmalloc(LZ4_MEM_COMPRESS);
	compressor_method_register(COMPRESSOR_LZ4, &method_lz4);
#endif /* CONFIG_DCM_LZ4 */
}
