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

#ifndef SECURITY_H
#define SECURITY_H

#include <linux/types.h>

/* Encryption/decryption functions for the 'Communication' flow */
size_t security_encrypt(void *plain_buf, size_t plain_buf_sz, void **enc_buf);
size_t security_decrypt(void *enc_buf, size_t enc_buf_sz, void **plain_buf);


void dcm_asf_test(void);


#endif /* SECURITY_H */
