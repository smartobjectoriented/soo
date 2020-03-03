/*
 * Copyright (C) 2014-2019 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef ASF_H
#define ASF_H

/* Perform a buffer encryption (AES).
 * The encrypted buffer ('enc_buf') is allocated inside ASF directly. It should
 * be freed by the user.
 *
 * 'plain_buf': Buffer to encrypt (plains data)
 * 'plain_buf_sz': Size of 'plain_buf'
 * 'enc_buf': Buffer with the encrypted data. It is allocated by ASF
 *
 * return the size of the 'enc_buf' or -1 in case of error
 */
int asf_encode(uint8_t *plain_buf, size_t plain_buf_sz, uint8_t **enc_buf);


/* Performs a buffer decryption (AES).
 * The decrypted buffer ('plain_buf') is allocated inside ASF directly. It should
 * be freed by the user.
 *
 * 'enc_buf': Buffer to decrypt (encrypted data)
 * 'enc_buf_sz': Size of 'plain_buf'
 * 'plain_buf': Buffer with the decrypted data (plain  data). It is allocated by ASF
 *
 * return the size of the 'plain_buf' decoded buffer or -1 in case of error
 */
int asf_decode(uint8_t *enc_buf, size_t enc_buf_sz, uint8_t **plain_buf);


#endif /* ASF_H */
