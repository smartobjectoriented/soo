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

#ifndef SDIO_OPS_H
#define SDIO_OPS_H

struct mmc_card;
struct mmc_host;
struct sdio_func;

typedef enum {
	RT_MMC_IO_DIRECT = 1,
	RT_MMC_IO_EXTENDED
} rt_mmc_io_type_t;

typedef struct {
	volatile rt_mmc_io_type_t type;
	union {
		struct {
			struct mmc_host *host;
			volatile int write;
			volatile unsigned fn;
			volatile unsigned addr;
			volatile u8 in;
			u8 *out;
		} direct;
		struct {
			struct mmc_card *card;
			volatile int write;
			volatile unsigned fn;
			volatile unsigned addr;
			volatile int incr_addr;
			u8 *buf;
			volatile unsigned blocks;
			volatile unsigned blksz;
		} extended;
	} data;
	volatile int ret;
} rt_mmc_io_data_t;

int rtdm_mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn, unsigned addr, u8 in, u8 *out);
int rtdm_mmc_io_rw_extended(struct mmc_card *card, int write, unsigned fn, unsigned addr, int incr_addr, u8 *buf, unsigned blocks, unsigned blksz);
void rtdm_init_sdio_ops(void);

int rtdm_mmc_send_io_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr);

int rtdm_mmc_io_rw_direct_host(struct mmc_host *host, int write, unsigned fn, unsigned addr, u8 in, u8 *out);

int rtdm_sdio_io_rw_ext_helper(struct sdio_func *func, int write, unsigned addr, int incr_addr, u8 *buf, unsigned size);

void rtdm_propagate_mmc_io_rw(void);

extern volatile rt_mmc_io_data_t rt_mmc_io_data;

#endif /* SDIO_OPS_H */
