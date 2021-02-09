/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <linux/types.h>

#define COMPRESSOR_NO_COMPRESSION	0
#define COMPRESSOR_LZ4			1
#define COMPRESSOR_N_METHODS		2

typedef struct {

	uint8_t	compression_method;

	size_t	decompressed_size;

	/*
	 * First byte of the payload. Accessing to its address gives a direct access to the
	 * payload buffer.
	 */
	uint8_t	payload[0];

} compressor_data_t;

typedef struct {
	/* Function to be called when compressing data */
	int (*compress_callback)(void **data_compressed, void *source_data, size_t source_size);

	/* Function to be called when decompressing data */
	int (*decompress_callback)(void **data_decompressed, compressor_data_t *data_compressed, size_t compressed_size);

} compressor_method_t;

int compress_data(uint8_t method, void **data_compressed, void *source_data, size_t source_size);
int decompress_data(void **data_decompressed, void *data_compressed, size_t compressed_size);

void compressor_method_register(uint8_t method, compressor_method_t *method_desc);
void compressor_init(void);

#endif /* COMPRESSOR_H */
