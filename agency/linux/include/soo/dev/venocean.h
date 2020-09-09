/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VENOCEAN_H
#define VENOCEAN_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <linux/vt_kern.h>

#define VENOCEAN_NAME	"venocean"
#define VENOCEAN_PREFIX	"================== [" VENOCEAN_NAME "] "

#define ENOCEAN_UART5_DEV "ttyAMA5"

#define VENOCEAN_FRAME_SIZE 20

#define VENOCEAN_INIT_SIZE 5

//different states for the switchs (single channel)
//Interpreted from the frame during successive tests (could not find documentation about the data encription)
#define SWITCH_IS_RELEASED 0x00 
#define SWITCH_IS_UP 0x70
#define SWITCH_IS_DOWN 0x50


#define UP_GPIO		26

#define DOWN_GPIO	16

/* GPIO polling period expressed in ms */
#define GPIO_POLL_PERIOD	100

/*  This is a reserved char code we use to query (patched) Qemu to retrieve the window size. */
#define SERIAL_GWINSZ   '\254'

typedef struct {
	char c;
} venocean_request_t;

typedef struct {
	char c;
} venocean_response_t;

/* ASCII data coming from the enocean module*/
typedef struct {
	char	frame_begin;	/* 0x55 */
	char	data_length[2];	/* lenght for the main datas*/
	char	optionnal_length;	/* length for the optionnal datas */
	char	packet_type;	/* 0x01 = ERP Radio Telegramm */
	char	crc_h;	/* CRC 8 */
	char	command_code;	/* command code */
	char	switch_data;	/* switch state at telegramm reception*/
	char	switch_ID[4];	/* enocean device ID */
	char	secondary_datas[7];	/* secondary datas from the switch */
	char	crc_l;	/* CRC 8*/
} venocean_ascii_data_t;

DEFINE_RING_TYPES(venocean, venocean_request_t, venocean_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	spinlock_t ring_lock;
	venocean_back_ring_t ring;
	unsigned int irq;

} venocean_t;

bool venocean_ready(void);

static inline venocean_t *to_venocean(struct vbus_device *vdev) {
	vdevback_t *vdevback = dev_get_drvdata(&vdev->dev);
	return container_of(vdevback, venocean_t, vdevback);
}


#endif /* VENOCEAN_H */
