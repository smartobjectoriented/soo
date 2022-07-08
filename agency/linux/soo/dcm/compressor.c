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
#include <linux/gfp.h>

#include <asm/pgtable.h>
#include <asm/string.h>

#include <soo/uapi/dcm.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#include <linux/lz4.h>

static void *lz4_ctx;

static int lz4_compress_data(void **data_compressed, void *source_data, size_t source_size) {

	/* lz4_compressbound gives the worst reachable size after compression */
	compressor_data_t *alloc_data;
	size_t size_compressed;

	alloc_data = vmalloc(LZ4_compressBound(source_size) + sizeof(compressor_data_t));
	BUG_ON(!alloc_data);

	if ((size_compressed = LZ4_compress_default((const char *) source_data, alloc_data->payload, source_size,
						    LZ4_compressBound(source_size), lz4_ctx)) < 0) {
		lprintk("Error when compressing the ME\n");
		BUG();
	}

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

	alloc_buffer = __vmalloc(decompressed_size, GFP_HIGHUSER | __GFP_ZERO);
	BUG_ON(alloc_buffer == NULL);

	if (LZ4_decompress_fast(data->payload, alloc_buffer, decompressed_size) < 0) {
		lprintk("Error when decompressing the ME\n");
		BUG();
	}
	*data_decompressed = alloc_buffer;

	DBG("(Guessed) compressed size: %d, decompressed size: %d\n", compressed_size, decompressed_size);

	return decompressed_size;
}

int compress_data(void **data_compressed, void *source_data, size_t source_size) {
	return lz4_compress_data(data_compressed, source_data, source_size);
}

void decompress_data(void **data_decompressed, void *data_compressed, size_t compressed_size) {
	compressor_data_t *data = (compressor_data_t *) data_compressed;

	lz4_decompress_data(data_decompressed, data, compressed_size);

}

void compressor_init(void) {
	lz4_ctx = vmalloc(LZ4_MEM_COMPRESS);
	BUG_ON(!lz4_ctx);
}
