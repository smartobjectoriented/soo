/*
 * Allwinner SUNXI "glue layer"
 *
 * Copyright © 2015 Hans de Goede <hdegoede@redhat.com>
 * Copyright © 2013 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * Based on the sw_usb "Allwinner OTG Dual Role Controller" code.
 *  Copyright 2007-2012 (C) Allwinner Technology Co., Ltd.
 *  javen <javen@allwinnertech.com>
 *
 * Based on the DA8xx "glue layer" code.
 *  Copyright (c) 2008-2009 MontaVista Software, Inc. <source@mvista.com>
 *  Copyright (C) 2005-2006 by Texas Instruments
 *
 * This file is part of the Inventra Controller Driver for Linux.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */
#include <common.h>
#include <fastboot.h>

#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/gpio.h>
#include <asm/arch/usb_phy.h>
#include <asm-generic/gpio.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <linux/usb/musb.h>
#include "linux-compat.h"
#include "musb_core.h"
#include "musb_uboot.h"

/******************************************************************************
 ******************************************************************************
 * From the Allwinner driver
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************
 * From include/sunxi_usb_bsp.h
 ******************************************************************************/

/* reg offsets */
#define  USBC_REG_o_ISCR	0x0400
#define  USBC_REG_o_PHYCTL	0x0404
#define  USBC_REG_o_PHYBIST	0x0408
#define  USBC_REG_o_PHYTUNE	0x040c

#define  USBC_REG_o_VEND0	0x0043

/* Interface Status and Control */
#define  USBC_BP_ISCR_VBUS_VALID_FROM_DATA	30
#define  USBC_BP_ISCR_VBUS_VALID_FROM_VBUS	29
#define  USBC_BP_ISCR_EXT_ID_STATUS		28
#define  USBC_BP_ISCR_EXT_DM_STATUS		27
#define  USBC_BP_ISCR_EXT_DP_STATUS		26
#define  USBC_BP_ISCR_MERGED_VBUS_STATUS	25
#define  USBC_BP_ISCR_MERGED_ID_STATUS		24

#define  USBC_BP_ISCR_ID_PULLUP_EN		17
#define  USBC_BP_ISCR_DPDM_PULLUP_EN		16
#define  USBC_BP_ISCR_FORCE_ID			14
#define  USBC_BP_ISCR_FORCE_VBUS_VALID		12
#define  USBC_BP_ISCR_VBUS_VALID_SRC		10

#define  USBC_BP_ISCR_HOSC_EN			7
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT	6
#define  USBC_BP_ISCR_ID_CHANGE_DETECT		5
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT	4
#define  USBC_BP_ISCR_IRQ_ENABLE		3
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT_EN	2
#define  USBC_BP_ISCR_ID_CHANGE_DETECT_EN	1
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT_EN	0

/******************************************************************************
 * From usbc/usbc.c
 ******************************************************************************/

static u32 USBC_WakeUp_ClearChangeDetect(u32 reg_val)
{
	u32 temp = reg_val;

	temp &= ~(1 << USBC_BP_ISCR_VBUS_CHANGE_DETECT);
	temp &= ~(1 << USBC_BP_ISCR_ID_CHANGE_DETECT);
	temp &= ~(1 << USBC_BP_ISCR_DPDM_CHANGE_DETECT);

	return temp;
}

static void USBC_EnableIdPullUp(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val |= (1 << USBC_BP_ISCR_ID_PULLUP_EN);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_EnableDpDmPullUp(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val |= (1 << USBC_BP_ISCR_DPDM_PULLUP_EN);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceIdToLow(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_ID);
	reg_val |= (0x02 << USBC_BP_ISCR_FORCE_ID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceIdToHigh(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_ID);
	reg_val |= (0x03 << USBC_BP_ISCR_FORCE_ID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceVbusValidToLow(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val |= (0x02 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceVbusValidToHigh(__iomem void *base)
{
	u32 reg_val;

	reg_val = musb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val |= (0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	musb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ConfigFIFO_Base(void)
{
	u32 reg_value;

	/* config usb fifo, 8kb mode */
	reg_value = readl(SUNXI_SRAMC_BASE + 0x04);
	reg_value &= ~(0x03 << 0);
	reg_value |= (1 << 0);
	writel(reg_value, SUNXI_SRAMC_BASE + 0x04);
}

/******************************************************************************
 * Needed for the DFU polling magic
 ******************************************************************************/

static u8 last_int_usb;

bool dfu_usb_get_reset(void)
{
	return !!(last_int_usb & MUSB_INTR_RESET);
}

/******************************************************************************
 * MUSB Glue code
 ******************************************************************************/

static irqreturn_t sunxi_musb_interrupt(int irq, void *__hci)
{
	struct musb		*musb = __hci;
	irqreturn_t		retval = IRQ_NONE;

	/* read and flush interrupts */
	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB);
	last_int_usb = musb->int_usb;
	if (musb->int_usb)
		musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX);
	if (musb->int_tx)
		musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX);
	if (musb->int_rx)
		musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval |= musb_interrupt(musb);

	return retval;
}

/* musb_core does not call enable / disable in a balanced manner <sigh> */
static bool enabled = false;

static int sunxi_musb_enable(struct musb *musb)
{
	int ret;

	pr_debug("%s():\n", __func__);

	musb_ep_select(musb->mregs, 0);
	musb_writeb(musb->mregs, MUSB_FADDR, 0);

	if (enabled)
		return 0;

	/* select PIO mode */
	musb_writeb(musb->mregs, USBC_REG_o_VEND0, 0);

	if (is_host_enabled(musb)) {
		ret = sunxi_usb_phy_vbus_detect(0);
		if (ret == 1) {
			printf("A charger is plugged into the OTG: ");
			return -ENODEV;
		}
		ret = sunxi_usb_phy_id_detect(0);
		if (ret == 1) {
			printf("No host cable detected: ");
			return -ENODEV;
		}
		sunxi_usb_phy_power_on(0); /* port power on */
	}

	USBC_ForceVbusValidToHigh(musb->mregs);

	enabled = true;
	return 0;
}

static void sunxi_musb_disable(struct musb *musb)
{
	pr_debug("%s():\n", __func__);

	if (!enabled)
		return;

	if (is_host_enabled(musb))
		sunxi_usb_phy_power_off(0); /* port power off */

	USBC_ForceVbusValidToLow(musb->mregs);
	mdelay(200); /* Wait for the current session to timeout */

	enabled = false;
}

static int sunxi_musb_init(struct musb *musb)
{
	struct sunxi_ccm_reg *ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	pr_debug("%s():\n", __func__);

	musb->isr = sunxi_musb_interrupt;

	setbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_USB0);
#ifdef CONFIG_SUNXI_GEN_SUN6I
	setbits_le32(&ccm->ahb_reset0_cfg, 1 << AHB_GATE_OFFSET_USB0);
#endif
	sunxi_usb_phy_init(0);

	USBC_ConfigFIFO_Base();
	USBC_EnableDpDmPullUp(musb->mregs);
	USBC_EnableIdPullUp(musb->mregs);

	if (is_host_enabled(musb)) {
		/* Host mode */
		USBC_ForceIdToLow(musb->mregs);
	} else {
		/* Peripheral mode */
		USBC_ForceIdToHigh(musb->mregs);
	}
	USBC_ForceVbusValidToHigh(musb->mregs);

	return 0;
}

static const struct musb_platform_ops sunxi_musb_ops = {
	.init		= sunxi_musb_init,
	.enable		= sunxi_musb_enable,
	.disable	= sunxi_musb_disable,
};

static struct musb_hdrc_config musb_config = {
	.multipoint     = 1,
	.dyn_fifo       = 1,
	.num_eps        = 6,
	.ram_bits       = 11,
};

static struct musb_hdrc_platform_data musb_plat = {
	.mode		= MUSB_PERIPHERAL,
	.config         = &musb_config,
	.power          = 250,
	.platform_ops	= &sunxi_musb_ops,
};

void sunxi_musb_board_init(void)
{
	musb_register(&musb_plat, NULL, (void *)SUNXI_USB0_BASE);

	fastboot_init((void *) CONFIG_FASTBOOT_BUF_ADDR, CONFIG_FASTBOOT_BUF_SIZE);
}
