/*
 * Copyright (C) 2017-2018 Baptiste Delporte <bonel@bonel.net>
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

#ifdef CONFIG_MACH_SUN50I

#include <asm/io.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#define SUNXI_GPIO_CTRL_REG_PHYS	0x01c20800
#define SUNXI_GPIO_CTRL_REG_VIRT	0xf7c20800
#define SUNXI_GPIO_PC_CFG_REG		(2 * 0x24)
#define SUNXI_GPIO_PC_CFG_REG_ADDR	(SUNXI_GPIO_CTRL_REG_VIRT + SUNXI_GPIO_PC_CFG_REG)
#define SUNXI_GPIO_PC_DATA_REG		(2 * 0x24 + 0x10)
#define SUNXI_GPIO_PC_DATA_REG_ADDR	(SUNXI_GPIO_CTRL_REG_VIRT + SUNXI_GPIO_PC_DATA_REG)

void ll_gpio_set(int pin, int value) {
	unsigned int regval;
	volatile void *addr = (void *) SUNXI_GPIO_PC_DATA_REG_ADDR;

	if ((pin < 0) || (pin > 7))
		return;

	regval = readl(addr);

	if (value & 0x1)
		writel(regval | (1 << pin), addr);
	else
		writel(regval & (~(1 << pin)), addr);
}

void ll_gpio_init(void) {
	volatile void *addr = (void *) SUNXI_GPIO_PC_CFG_REG_ADDR;

	/* Configure PC2, PC3, PC4 and PC7 as outputs */
	writel((1 << 8) | (1 << 12) | (1 << 16) | (1 << 28), addr);
}

int map_ll_gpio(struct map_desc *map) {
	map->pfn = __phys_to_pfn(SUNXI_GPIO_CTRL_REG_PHYS);
	map->virtual = (SUNXI_GPIO_CTRL_REG_VIRT);
	map->virtual &= PAGE_MASK;
	map->length = PAGE_SIZE;
	map->type = MT_DEVICE;

	return 1;
}
#endif /* CONFIG_MACH_SUN50I */
