/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef BANDWITDH_H
#define BANDWITDH_H

#if defined(CONFIG_SOO_AGENCY)

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#define N_BANDWIDTH_SLOTS	8
#define N_BANDWIDTH_DELAYS	4000

int ll_bandwidth_collect_delay(uint32_t index);
void ll_bandwidth_collect_delay_show(uint32_t index, size_t size);

int rtdm_ll_bandwidth_collect_delay(uint32_t index);
void rtdm_ll_bandwidth_collect_delay_show(uint32_t index, size_t size);

void ll_bandwidth_reset_delays(uint32_t index);

void ll_bandwidth_init(void);

/* NEON function */

void ll_bandwidth_compute(s64 *delays, size_t size, uint32_t *div, uint32_t *result);

#endif /* CONFIG_SOO_AGENCY */

#endif /* BANDWITDH_H */
