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

#ifndef SDIO_IO_H
#define SDIO_IO_H

u8 rtdm_sdio_readb(struct sdio_func *func, unsigned int addr, int *err_ret);
void rtdm_sdio_writeb(struct sdio_func *func, u8 b, unsigned int addr, int *err_ret);
int rtdm_sdio_readsb(struct sdio_func *func, void *dst, unsigned int addr, int count);
int rtdm_sdio_writesb(struct sdio_func *func, unsigned int addr, void *src, int count);

#endif /* SDIO_IO_H */

