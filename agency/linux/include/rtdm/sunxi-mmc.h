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

#ifndef SUNXI_MMC_H
#define SUNXI_MMC_H

#include <linux/mmc/host.h>

struct sunxi_mmc_host;
struct mmc_data;
struct mmc_request;

void rtdm_sunxi_sdio_start_thread(void);
void rtdm_sunxi_sdio_init_funcs(void);
void rtdm_sunxi_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq);
void rtdm_sunxi_request_irq(void);


int sunxi_mmc_map_dma(struct sunxi_mmc_host *host, struct mmc_data *data);

enum dma_data_direction sunxi_mmc_get_dma_dir(struct mmc_data *data);
void sunxi_mmc_start_dma(struct sunxi_mmc_host *host, struct mmc_data *data);

void rtdm_sunxi_mmc_init(void);
void rtdm_sunxi_mmc_init_threads(void);
void sunxi_mmc_host_init(struct sunxi_mmc_host *mmc_host);

void sunxi_mmc_dump_errinfo(struct sunxi_mmc_host *host);

#endif /* SUNXI_MMC_H */


