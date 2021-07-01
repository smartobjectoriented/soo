/*******************************************************************
 * rpisense.h
 *
 * Copyright (c) 2020 HEIG-VD, REDS Institute
 *******************************************************************/


#ifndef RPISENSE_H
#define RPISENSE_H

#include <linux/types.h>
#include <linux/platform_device.h>

typedef void(*joystick_handler_t)(struct platform_device *pdev, int key);

#define UP      0x04
#define DOWN    0x01
#define RIGHT   0x02
#define LEFT    0x10
#define CENTER  0x08

void rpisense_init(void);

void display_led(int led_nr, bool on);

void rpisense_joystick_handler_register(struct platform_device *pdev, joystick_handler_t joystick_handler);


#endif /* RPISENSE_H */
