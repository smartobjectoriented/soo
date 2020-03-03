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

#ifndef CORE_MMC_H
#define CORE_MMC_H

struct mmc_host;
struct mmc_request;
struct mmc_command;

struct mmc_host *get_current_host(void);

void rtdm_mmc_request_done(struct mmc_host *host, struct mmc_request *mrq);
void rtdm_mmc_wait_for_req(struct mmc_host *host, struct mmc_request *mrq);
int rtdm_mmc_wait_for_cmd(struct mmc_host *host, struct mmc_command *cmd, int retries);

#endif /* CORE_MMC_H */
