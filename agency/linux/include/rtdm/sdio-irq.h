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

#ifndef SDIO_IRQ_H
#define SDIO_IRQ_H

struct sdio_func;

void rtdm_sdio_start_thread(void *_host);
void rtdm_sdio_init_funcs(void *_host);
void rtdm_sdio_irq(void *_host);

void rtdm_wlan_sdio_interrupt(struct sdio_func *func);
void rtdm_btmrvl_sdio_interrupt(struct sdio_func *func);
void rtdm_sdio_irq_init_threads(struct mmc_host *host);

bool in_rtdm_sdio_irq_context(struct sdio_func *func);

#endif /* SDIO_IRQ_H */


