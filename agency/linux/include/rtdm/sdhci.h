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

#ifndef BCM2835_H
#define BCM2835_H

#include <linux/netdevice.h>

struct mmc_request;
struct mmc_host;

netdev_tx_t brcmf_netdev_start_xmit(struct sk_buff *skb, struct net_device *ndev);

void rtdm_sdhci_mmc_init_threads(void);
void rtdm_sdhci_mmc_init(void);
void rtdm_sdhci_request_irq(void);

void rtdm_sdhci_request_irq(void);

void sdhci_request(struct mmc_host *mmc, struct mmc_request *mrq);

void rtdm_sdhci_sdio_init_funcs(void);
#endif /* BCM2835_H */


