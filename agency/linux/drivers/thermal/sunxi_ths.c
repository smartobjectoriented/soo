/*
 * Copyright (C) 2017 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>

#include <asm/delay.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#define THS_CLK_RATE					4000000
#define THS_BUS_CLK_RATE				2600000

#define SUNXI_THERMAL_PHYS				0x01c25000
#define SUNXI_THERMAL_THS_CTRL0_REG			0x00
#define SUNXI_THERMAL_THS_CTRL1_REG			0x04
#define SUNXI_THERMAL_ADC_CDAT_REG			0x14
#define SUNXI_THERMAL_THS_CTRL2_REG			0x40
#define SUNXI_THERMAL_THS_INT_CTRL_REG			0x44
#define SUNXI_THERMAL_THS_FILTER_REG			0x70
#define SUNXI_THERMAL_THS0_1_CDATA			0x74
#define SUNXI_THERMAL_THS2_CDATA			0x78
#define SUNXI_THERMAL_THS_STAT_REG			0x48
#define SUNXI_THERMAL_THS0_DATA_REG			0x80
#define SUNXI_THERMAL_THS1_DATA_REG			0x84
#define SUNXI_THERMAL_THS2_DATA_REG			0x88

#define SUNXI_CCU_PHYS					0x01c20000
#define SUNXI_BUS_SOFT_RESET_REGISTER3_REG		0x02d0

#define DELAY_MS					5
#define THREAD_POLL_PERIOD_MS				2000

#define SUNXI_THERMAL_B					2170000
#define SUNXI_THERMAL_A					1000
#define SUNXI_THERMAL_DIV				8560

#define SHUTDOWN_ALERT_TEMP				90

typedef struct {
	struct device		*dev;
	void __iomem		*base;
	struct clk		*ths_clk;
	struct clk		*bus_ths_clk;
	void volatile		*bsr_register;
	struct task_struct	*watcher_thread;
} sunxi_ths_t;

static sunxi_ths_t *sunxi_ths = NULL;

static void sunxi_ths_reset_off(void) {
	int bsr_value;

	bsr_value = readl(sunxi_ths->bsr_register);
	bsr_value = bsr_value & (~(1 << 8));
	writel(bsr_value, sunxi_ths->bsr_register);
}

static void sunxi_ths_reset_on(void) {
	int bsr_value;

	bsr_value = readl(sunxi_ths->bsr_register);
	bsr_value = bsr_value | (1 << 8);
	writel(bsr_value, sunxi_ths->bsr_register);
}

static void sunxi_ths_reset(void) {
	int i;

	sunxi_ths_reset_off();

	for (i = 0 ; i < DELAY_MS ; i++)
		udelay(1000);

	sunxi_ths_reset_on();
}

static void sunxi_ths_init(void) {
	int reg, i;
	int cal;

	/* Start calibration */
	writel(1 << 17, sunxi_ths->base + SUNXI_THERMAL_THS_CTRL1_REG);
	for (i = 0 ; i < DELAY_MS ; i++)
		udelay(1000);

	/* Read calibration data */
	cal = readl(sunxi_ths->base + SUNXI_THERMAL_ADC_CDAT_REG);

	/* Write calibration data */
	writel((cal << 16) | cal, sunxi_ths->base + SUNXI_THERMAL_THS0_1_CDATA);
	writel(cal, sunxi_ths->base + SUNXI_THERMAL_THS2_CDATA);

	/* Set ADC acquire time */
	writel(0x190, sunxi_ths->base + SUNXI_THERMAL_THS_CTRL0_REG);

	/* Set sensor acquire time (SENSOR_ACQ1) */
	writel(0x01900000, sunxi_ths->base + SUNXI_THERMAL_THS_CTRL2_REG);

	/* Automatically clear interrupts */
	writel(0x777, sunxi_ths->base + SUNXI_THERMAL_THS_STAT_REG);

	/* Enable */
	writel(0x06, sunxi_ths->base + SUNXI_THERMAL_THS_FILTER_REG);

	/* No interrupts */
	writel(0, sunxi_ths->base + SUNXI_THERMAL_THS_INT_CTRL_REG);

	/* Enable sensors 0, 1 and 2 */
	reg = readl(sunxi_ths->base + SUNXI_THERMAL_THS_CTRL2_REG);
	writel(reg | (1 << 0) | (1 << 1) | (1 << 2), sunxi_ths->base + SUNXI_THERMAL_THS_CTRL2_REG);
}

int sunxi_ths_get_i(unsigned i) {
	if (i >= 3)
		return -EINVAL;

	return readl(sunxi_ths->base + SUNXI_THERMAL_THS0_DATA_REG + i * sizeof(int));
}

int sunxi_ths_get(void) {
	return sunxi_ths_get_i(0);
}

int sunxi_ths_get_temp(void) {
	return (SUNXI_THERMAL_B - sunxi_ths_get() * SUNXI_THERMAL_A) / SUNXI_THERMAL_DIV;
}

void sunxi_ths_dump(void) {
	lprintk("SUNXI_THERMAL_THS_CTRL0_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_CTRL0_REG));
	lprintk("SUNXI_THERMAL_THS_CTRL1_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_CTRL1_REG));
	lprintk("SUNXI_THERMAL_ADC_CDAT_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_ADC_CDAT_REG));
	lprintk("SUNXI_THERMAL_THS_CTRL2_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_CTRL2_REG));
	lprintk("SUNXI_THERMAL_THS_INT_CTRL_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_INT_CTRL_REG));
	lprintk("SUNXI_THERMAL_THS_FILTER_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_FILTER_REG));
	lprintk("SUNXI_THERMAL_THS_STAT_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS_STAT_REG));
	lprintk("SUNXI_THERMAL_THS0_DATA_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS0_DATA_REG));
	lprintk("SUNXI_THERMAL_THS1_DATA_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS1_DATA_REG));
	lprintk("SUNXI_THERMAL_THS2_DATA_REG: %08x\n", readl(sunxi_ths->base + SUNXI_THERMAL_THS2_DATA_REG));
}

static void show_uptime(void) {
	struct timespec uptime;
	struct timespec idle;
	u64 idletime;
	u64 nsec;
	u32 rem;
	int i;

	idletime = 0;
	for_each_possible_cpu(i)
		idletime += (__force u64) kcpustat_cpu(i).cpustat[CPUTIME_IDLE];

	get_monotonic_boottime(&uptime);

	nsec = (__force u64) kcpustat_cpu(i).cpustat[CPUTIME_IDLE];

	idle.tv_sec = div_u64_rem(nsec, NSEC_PER_SEC, &rem);
	idle.tv_nsec = rem;
	lprintk("%lu.%02lu %lu.%02lu\n",
		(unsigned long) uptime.tv_sec, (uptime.tv_nsec / (NSEC_PER_SEC / 100)),
		(unsigned long) idle.tv_sec, (idle.tv_nsec / (NSEC_PER_SEC / 100)));
}

void sunxi_ths_show_temperature(void) {
	int temp;

	if (unlikely(!sunxi_ths))
		return;

	temp = sunxi_ths_get_temp();

	lprintk("Temperature: %d\n", temp);
}

static int sunxi_ths_thread(void *arg) {
	int temp;

	while(1) {
		msleep(THREAD_POLL_PERIOD_MS);

		temp = sunxi_ths_get_temp();

#ifdef DEBUG
		lprintk("Temp: %d\n", temp);
#endif

		if (temp >= SHUTDOWN_ALERT_TEMP) {
			lprintk("CRITICAL! Temperature: %d\n", temp);
			show_uptime();

			BUG();
		}
	}

	return 0;
}

static int sunxi_ths_probe(struct platform_device *pdev) {
	struct resource *res;
	void __iomem *regs;

	sunxi_ths = devm_kzalloc(&pdev->dev, sizeof(*sunxi_ths), GFP_KERNEL);
	if (!sunxi_ths)
		return -ENOMEM;
	platform_set_drvdata(pdev, sunxi_ths);
	sunxi_ths->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	sunxi_ths->base = regs;

	DBG("sunxi_ths->base: %08x\n", sunxi_ths->base);

	/* bus-THS clock */
	sunxi_ths->bus_ths_clk = devm_clk_get(&pdev->dev, "bus-ths");
	clk_prepare_enable(sunxi_ths->bus_ths_clk);

	if (IS_ERR(sunxi_ths->bus_ths_clk)) {
		dev_err(&pdev->dev, "Can't get bus-ths clock\n");
		goto fail_bus_ths;
	}

	/* THS clock */
	sunxi_ths->ths_clk = devm_clk_get(&pdev->dev, "ths");
	clk_prepare_enable(sunxi_ths->ths_clk);

	clk_set_rate(sunxi_ths->ths_clk, THS_CLK_RATE);
	if (IS_ERR(sunxi_ths->ths_clk)) {
		dev_err(&pdev->dev, "Can't get ths clock\n");
		goto fail_ths;
	}

	sunxi_ths->bsr_register = ioremap(SUNXI_CCU_PHYS + SUNXI_BUS_SOFT_RESET_REGISTER3_REG, sizeof(int));

	sunxi_ths_reset();
	sunxi_ths_init();

	sunxi_ths->watcher_thread = kthread_run(sunxi_ths_thread, NULL, "THS");

	lprintk("sunxi_ths: probed and initialized.\n");

	return 0;

fail_ths:
	clk_disable_unprepare(sunxi_ths->ths_clk);

fail_bus_ths:
	clk_disable_unprepare(sunxi_ths->bus_ths_clk);

	return -1;
}

static int sunxi_ths_remove(struct platform_device *pdev) {
	clk_disable_unprepare(sunxi_ths->ths_clk);
	clk_disable_unprepare(sunxi_ths->bus_ths_clk);

	devm_kfree(&pdev->dev, sunxi_ths);

	return 0;
}

static const struct of_device_id sunxi_ths_match[] = {
	{ .compatible = "allwinner,thermal_sensor", },
	{  }
};
MODULE_DEVICE_TABLE(of, sunxi_ths_match);

static struct platform_driver sunxi_ths_driver = {
	.probe	= sunxi_ths_probe,
	.remove	= sunxi_ths_remove,
	.driver	= {
		.name		= "sunxi-ths",
		.of_match_table	= sunxi_ths_match,
	},
};
module_platform_driver(sunxi_ths_driver);
