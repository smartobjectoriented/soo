/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef __MACH_VEXPRESS_H__
#define __MACH_VEXPRESS_H__

#if 0
#define PERIPHERAL_CLOCK_RATE		2500000

/* For now, all mappings are flat (physical = virtual)
 */

/* Virtual address must be inside vmalloc area - this is weird - better
 * to create virtual mapping on the fly */
#define UART0_PHYS			0xE0000000
#define UART0_VIRT			0xFE000000

#define SCU_PERIPH_PHYS			0xF8F00000
#define SCU_PERIPH_VIRT			SCU_PERIPH_PHYS

/* The following are intended for the devices that are mapped early */

#define SCU_PERIPH_BASE			IOMEM(SCU_PERIPH_VIRT)
#define SCU_GLOBAL_TIMER_BASE		(SCU_PERIPH_BASE + 0x200)

#define SLCR_BASE_VIRT			0xf8000000
#define SLCR_BASE_PHYS			0xf8000000

#define SLCR_ARMPLL_CTRL		(SLCR_BASE_VIRT | 0x100)
#define SLCR_DDRPLL_CTRL		(SLCR_BASE_VIRT | 0x104)
#define SLCR_IOPLL_CTRL			(SLCR_BASE_VIRT | 0x108)
#define SLCR_PLL_STATUS			(SLCR_BASE_VIRT | 0x10c)
#define SLCR_ARMPLL_CFG			(SLCR_BASE_VIRT | 0x110)
#define SLCR_DDRPLL_CFG			(SLCR_BASE_VIRT | 0x114)
#define SLCR_IOPLL_CFG			(SLCR_BASE_VIRT | 0x118)
#define SLCR_ARM_CLK_CTRL		(SLCR_BASE_VIRT | 0x120)
#define SLCR_DDR_CLK_CTRL		(SLCR_BASE_VIRT | 0x124)
#define SLCR_DCI_CLK_CTRL		(SLCR_BASE_VIRT | 0x128)
#define SLCR_APER_CLK_CTRL		(SLCR_BASE_VIRT | 0x12c)
#define SLCR_GEM0_CLK_CTRL		(SLCR_BASE_VIRT | 0x140)
#define SLCR_GEM1_CLK_CTRL		(SLCR_BASE_VIRT | 0x144)
#define SLCR_SMC_CLK_CTRL		(SLCR_BASE_VIRT | 0x148)
#define SLCR_LQSPI_CLK_CTRL		(SLCR_BASE_VIRT | 0x14c)
#define SLCR_SDIO_CLK_CTRL		(SLCR_BASE_VIRT | 0x150)
#define SLCR_UART_CLK_CTRL		(SLCR_BASE_VIRT | 0x154)
#define SLCR_SPI_CLK_CTRL		(SLCR_BASE_VIRT | 0x158)
#define SLCR_CAN_CLK_CTRL		(SLCR_BASE_VIRT | 0x15c)
#define SLCR_DBG_CLK_CTRL		(SLCR_BASE_VIRT | 0x164)
#define SLCR_PCAP_CLK_CTRL		(SLCR_BASE_VIRT | 0x168)
#define SLCR_FPGA0_CLK_CTRL		(SLCR_BASE_VIRT | 0x170)
#define SLCR_FPGA1_CLK_CTRL		(SLCR_BASE_VIRT | 0x180)
#define SLCR_FPGA2_CLK_CTRL		(SLCR_BASE_VIRT | 0x190)
#define SLCR_FPGA3_CLK_CTRL		(SLCR_BASE_VIRT | 0x1a0)
#define SLCR_621_TRUE			(SLCR_BASE_VIRT | 0x1c4)

/*
 * Mandatory for CONFIG_LL_DEBUG, UART is mapped virtual = physical
 */

#define LL_UART_PADDR	UART0_PHYS
#define LL_UART_VADDR	UART0_VIRT

#endif /* 0 */

#define VEXPRESS_GIC_DIST_PHYS 	0x2c001000
#define VEXPRESS_GIC_DIST_SIZE  0x00001000

#define VEXPRESS_GIC_CPU_PHYS 	0x2c002000
#define VEXPRESS_GIC_CPU_SIZE		0x00001000

#define VEXPRESS_UART0_VIRT		  0xf8090000
#define VEXPRESS_UART0_PHYS	    0x1c090000

#define VEXPRESS_SYSREG_BASE	  0x1c010000
#define VEXPRESS_SYSREG_SIZE	  0x1000

/* vexpress-sysregs flags as defined in linux:drivers/mfd/vexpress-sysreg.c */

#define SYS_ID                  0x000
#define SYS_SW                  0x004
#define SYS_LED                 0x008
#define SYS_100HZ               0x024
#define SYS_FLAGSSET            0x030
#define SYS_FLAGSCLR            0x034
#define SYS_NVFLAGS             0x038
#define SYS_NVFLAGSSET          0x038
#define SYS_NVFLAGSCLR          0x03c
#define SYS_MCI                 0x048
#define SYS_FLASH               0x04c
#define SYS_CFGSW               0x058
#define SYS_24MHZ               0x05c
#define SYS_MISC                0x060
#define SYS_DMA                 0x064
#define SYS_PROCID0             0x084
#define SYS_PROCID1             0x088
#define SYS_CFGDATA             0x0a0
#define SYS_CFGCTRL             0x0a4
#define SYS_CFGSTAT             0x0a8

#define SYS_HBI_MASK            0xfff
#define SYS_PROCIDx_HBI_SHIFT   0

#define SYS_MCI_CARDIN          (1 << 0)
#define SYS_MCI_WPROT           (1 << 1)

#define SYS_MISC_MASTERSITE     (1 << 14)


#endif /* __MACH_VEXPRESS_H__ */
