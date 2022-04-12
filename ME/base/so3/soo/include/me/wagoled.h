/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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

#ifndef WAGOLED_H
#define WAGOLED_H

#include <completion.h>
#include <spinlock.h>
#include <printk.h>

#include <me/common.h>

#define MEWAGOLED_NAME		"ME wagoled"
#define MEWAGOLED_PREFIX	"[ " MEWAGOLED_NAME " ]"

/*** Compatible devices ***/
/*** PTM 210 ***/
#define BUTTON_ID 			0x00367BBB
#define BUTTON_ID_SIZE		0x4
#define SWITCH_IS_RELEASED 0x00 
#define SWITCH_IS_UP 0x70
#define SWITCH_IS_DOWN 0x50

/*** Data offesets ***/
#define RORG_OFFS			0x0
#define CMD_OFFS			0x1
#define ID_OFFS				0x02

/*** First byte of received data ***/
#define RORG_BYTE			0xF6

/*** Minimun data length ***/
#define MIN_LENGTH			0x06

/*** ROOMS ***/
#define ROOM1	0x0
#define ROOM2	0x1

/*
 * Never use lock (completion, spinlock, etc.) in the shared page since
 * the use of ldrex/strex instructions will fail with cache disabled.
 */
typedef struct {
	/*
	 * MUST BE the last field, since it contains a field at the end which is used
	 * as "payload" for a concatened list of hosts.
	 */
	me_common_t me_common;

} sh_wagoled_t;

/* Export the reference to the shared content structure */
extern sh_wagoled_t *sh_wagoled;

#define pr_err(fmt, ...) \
	do { \
		printk("[%s:%i] Error: "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
	} while(0)

#endif /* WAGOLED_H */


