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

#ifndef ASF_PRIV_H
#define ASF_PRIV_H

#include <linux/tee_drv.h>
#include <linux/optee_private.h>

#include <soo/uapi/console.h>
#include <soo/core/asf.h>

/* args of Hello world command */
typedef struct {
	int val;
} hello_args_t;


/* asf core functions */
int asf_open_session(struct tee_context **ctx, uint8_t *uuid);
int asf_close_session(struct tee_context *ctx, int session_id);

void asf_crypto_example(void);

void asf_crypto_large_buf_test(void);

int hello_world_ta_cmd(hello_args_t *args);

#endif /* ASF_PRIV_H */
