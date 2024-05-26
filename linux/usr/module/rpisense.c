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

#include "rpisense.h"

static struct rpisense *rpisense;

static struct task_struct *js_polling_th;

/* 8 (row) * 8 (column) * 3 (colors) + 1 (first byte needed blank) */
#define SIZE_FB 193


#define LED_ADDR	0x46

#define JOYSTICK_SUB_ADDR	0xF2

static joystick_handler_t __joystick_handler;
static struct platform_device *__pdev;

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

	i2c_master_send(rpisense->i2c_client, matrix, SIZE_FB);
}

/*
 * Polling the joystick event
 */

int joystick_poll(void *args) {
	int key = 0;
	int prev_key = 0;

	while (!kthread_should_stop()) {

		key = i2c_smbus_read_byte_data(rpisense->i2c_client, JOYSTICK_SUB_ADDR);
		msleep(50);

		/* According to the convention from the vext datasheet */
		switch (key){
		case CENTER:
			key = 1;
			break;
		case LEFT:
			key = 2;
			break;
		case DOWN:
			key = 5;
			break;
		case RIGHT:
			key = 4;
			break;
		case UP:
			key = 3;
			break;
		default:
			break;
		}

		if (prev_key != key) {
			if (__joystick_handler && __pdev)
				__joystick_handler(__pdev, key);
			prev_key = key;
		}
	}

	return 0;
}

void rpisense_joystick_handler_register(struct platform_device *pdev, joystick_handler_t joystick_handler) {
	__joystick_handler = joystick_handler;
	__pdev = pdev;
}

void rpisense_init(void) {
	int i;

	__joystick_handler = NULL;

	for (i = 0; i < SIZE_FB; i++)
		matrix[i] = 0;

	rpisense = rpisense_get_dev();

	js_polling_th = kthread_run(joystick_poll, NULL, "rpisense-joystick_fn");

}

void rpisense_exit(void) {
	kthread_stop(js_polling_th);
}
