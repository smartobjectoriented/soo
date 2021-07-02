/*******************************************************************
 * rpisense.c
 *
 * Copyright (c) 2020 HEIG-VD, REDS Institute
 *******************************************************************/

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/mfd/rpisense/core.h>

#include <linux/platform_device.h>

#include "rpisense-led.h"

static struct rpisense *rpisense = NULL;

/* 8 (row) * 8 (column) * 3 (colors) + 1 (first byte needed blank) */
#define SIZE_FB 193

/* LED address on the I2C bus */
#define LED_ADDR	0x46

uint8_t leds_array[SIZE_FB];

unsigned char matrix[SIZE_FB];

extern struct rpisense *rpisense_get_dev(void);

/* 5-bit per colour */
#define RED	{ 0x00, 0xf8 }
#define GREEN	{ 0xe0, 0x07 }
#define BLUE	{ 0x1f, 0x00 }
#define GRAY    { 0xef, 0x3d }

#define WHITE	{ 0xff, 0xff }
#define BLACK	{ 0x00, 0x00 }

unsigned char ledsoff[][2] = {
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
};

unsigned char leds[][64][2] = {
	{
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLUE, BLUE, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLUE, BLUE, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	},
	{
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, WHITE, WHITE, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, WHITE, WHITE, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	},
	{
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, GREEN, GREEN, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, GREEN, GREEN, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	},
	{
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, GRAY, GRAY, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, GRAY, GRAY, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	},
	{
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, RED, RED, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, RED, RED, BLACK, BLACK, BLACK,
		BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK,
	}
};

/* In drivers/video/fbdev/rpisense-fb.c */
void update_rpisense_fb_mem(uint8_t *matrix);

void display_led(int led_nr, bool on) {
	int i, j;
	u16 *mem = (u16 *) leds[led_nr];

	switch (on) {
	case true:
		for (j = 0; j < 8; j++) {
			for (i = 0; i < 8; i++) {
				matrix[(j * 24) + i + 1] |= (mem[(j * 8) + i] >> 11) & 0x1F;
				matrix[(j * 24) + (i + 8) + 1] |= (mem[(j * 8) + i] >> 6) & 0x1F;
				matrix[(j * 24) + (i + 16) + 1] |= mem[(j * 8) + i] & 0x1F;
			}
		}
		break;


	case false:
		for (j = 0; j < 8; j++) {
			for (i = 0; i < 8; i++) {
				matrix[(j * 24) + i + 1] &= ~((mem[(j * 8) + i] >> 11) & 0x1F);
				matrix[(j * 24) + (i + 8) + 1] &= ~((mem[(j * 8) + i] >> 6) & 0x1F);
				matrix[(j * 24) + (i + 16) + 1] &= ~(mem[(j * 8) + i] & 0x1F);
			}
		}
	}

	update_rpisense_fb_mem(matrix);

	i2c_master_send(rpisense->i2c_client, matrix, SIZE_FB);
}
EXPORT_SYMBOL(display_led);

void senseled_init(void) {
	int i;

	for (i = 0; i < SIZE_FB; i++)
		matrix[i] = 0;

	rpisense = rpisense_get_dev();

}
EXPORT_SYMBOL(senseled_init);
