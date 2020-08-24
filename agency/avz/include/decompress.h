/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef DECOMPRESS_H
#define DECOMPRESS_H

typedef int decompress_fn(unsigned char *inbuf, unsigned int len,
                          int (*fill)(void*, unsigned int),
                          int (*flush)(void*, unsigned int),
                          unsigned char *outbuf, unsigned int *posp,
                          void (*error)(const char *x));

/* inbuf   - input buffer
 * len     - len of pre-read data in inbuf
 * fill    - function to fill inbuf when empty
 * flush   - function to write out outbuf
 * outbuf  - output buffer
 * posp    - if non-null, input position (number of bytes read) will be
 *           returned here
 * error   - error reporting function
 *
 * If len != 0, inbuf should contain all the necessary input data, and fill
 * should be NULL
 * If len = 0, inbuf can be NULL, in which case the decompressor will allocate
 * the input buffer.  If inbuf != NULL it must be at least XXX_IOBUF_SIZE bytes.
 * fill will be called (repeatedly...) to read data, at most XXX_IOBUF_SIZE
 * bytes should be read per call.  Replace XXX with the appropriate decompressor
 * name, i.e. LZMA_IOBUF_SIZE.
 *
 * If flush = NULL, outbuf must be large enough to buffer all the expected
 * output.  If flush != NULL, the output buffer will be allocated by the
 * decompressor (outbuf = NULL), and the flush function will be called to
 * flush the output buffer at the appropriate time (decompressor and stream
 * dependent).
 */

decompress_fn bunzip2, unlzma;

int decompress(void *inbuf, unsigned int len, void *outbuf);

#endif /* DECOMPRESS_H */
