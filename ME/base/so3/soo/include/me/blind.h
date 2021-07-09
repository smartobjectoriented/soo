/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 David Truan <david.truan@heig-vd.ch>
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

#ifndef BLIND_H
#define BLIND_H

#include <types.h>
#include <ioctl.h>

#include <soo/soo.h>

#include <me/eco_stability.h>

/* Cooperation with SOO.outdoor */
#include <me/outdoor.h>

#define APP_NAME "blind"

/* Commands */
#define IOCTL_BLIND_UP		_IOW(0x500b118du, 1, uint32_t)
#define IOCTL_BLIND_DOWN	_IOW(0x500b118du, 2, uint32_t)
#define IOCTL_BLIND_STOP	_IOW(0x500b118du, 3, uint32_t)
#define IOCTL_SET_NAME		_IOW(0x500b118du, 5, uint32_t)
#define IOCTL_SET_ACTIVE	_IOW(0x500b118du, 6, uint32_t)
#define IOCTL_START_CALIBRATION	_IOW(0x500b118du, 7, uint32_t)
#define IOCTL_STOP_CALIBRATION	_IOW(0x500b118du, 8, uint32_t)
#define IOCTL_SET_LOW_LIGHT	_IOW(0x500b118du, 9, uint32_t)
#define IOCTL_SET_STRONG_LIGHT	_IOW(0x500b118du, 10, uint32_t)
#define IOCTL_SET_STRONG_WIND	_IOW(0x500b118du, 11, uint32_t)
#define IOCTL_SET_POS		_IOW(0x500b118du, 12, uint32_t)

#define BLIND_MAX_NAME_SIZE	16
#define BLIND_MAX_DESC	6

/* Generic constants */

#define BLIND_NO_CMD	0
#define MAX_BUF_CMDS	8

#define UP	true
#define DOWN	false

/*
 * The blind position is coded between 0 and 100:
 * - Fully open = completely up
 * - Fully closed = completely down
 */
#define FULLY_OPEN_POS		100
#define FULLY_CLOSED_POS	0

/* Position precision margin */
#define POS_MARGIN	5

/* Motor default speed in % */
#define DEFAULT_SPEED	50

typedef enum {
	STOPPED = 0,
	GOING_UP,
	GOING_DOWN,
	PROCESSING
} blind_state_t;

typedef enum {
	NO_CALIB = 0,
	CALIB_UP,
	CALIB_DOWN
} calibration_step_t;

typedef struct {

	/* Times used to automate the travel after calibration, expressed in us */
	uint64_t	total_time;	/* Time between completely up and completely down positions */
	uint64_t	traveled_time;	/* Effective time */

	/* Position of the blind, between FULLY_OPEN_POS and FULLY_CLOSED_POS */
	uint8_t		requested_blind_position;	/* Command */
	uint8_t		real_blind_position;		/* Effective state */
	blind_state_t	requested_blind_state;	/* Command */
	blind_state_t	real_blind_state;	/* Effective state */

	/* Used to know if we currently are doing the calibration process */
	bool		doing_calibration;
	bool		calibration_done;

	/* Synergy with SOO.outdoor Smart Objects */
	uint32_t	strong_light;	/* Light in lx over which the blind is automatically up (0 = disabled) */
	uint32_t	low_light;	/* Light in lx below which the blind is automatically down (0 = disabled) */
	uint32_t	strong_wind;	/* Wind speed in m/s required to down the blind (0 = disabled) */

} blind_desc_t;

/* The global blind info contains the blind descriptors and the ID of this Smart Object */
typedef struct {
	blind_desc_t	blind[MAX_DESC];
	uint8_t		my_id;

	/* SOO Presence Behavioural Pattern data */
	soo_presence_data_t	presence[MAX_DESC];
} blind_info_t;

/* The global blind data contains the global blind info */
typedef struct {
	blind_info_t	info;
	uint8_t		origin_agencyUID[SOO_AGENCY_UID_SIZE];
} blind_data_t;

/* vUIHandler command */
typedef struct {
	uint32_t	cmd;
	uint32_t	arg;
} blind_cmd_t;

/* vUIHandler packet */
typedef struct {
	uint8_t		type;
	/* /uint32_t	age; */
	uint8_t		payload[0];
} blind_vuihandler_pkt_t;

#define BLIND_VUIHANDLER_HEADER_SIZE	sizeof(blind_vuihandler_pkt_t)
#define BLIND_VUIHANDLER_DATA_SIZE	sizeof(blind_info_t)

extern uint8_t SOO_blind_spid[SPID_SIZE];

extern void *localinfo_data;
extern blind_data_t *blind_data;
extern blind_info_t *tmp_blind_info;
extern blind_desc_t *tmp_blind_desc;

extern uint32_t my_id;

extern agencyUID_t origin_agencyUID;
extern char origin_soo_name[SOO_NAME_SIZE];
extern agencyUID_t target_agencyUID;
extern char target_soo_name[SOO_NAME_SIZE];

extern bool available_devcaps;
extern bool remote_app_connected;
extern bool can_migrate;
extern bool has_migrated;

extern spinlock_t blind_lock;

void blind_start_threads(void);
void create_my_desc(void);

void blind_enable_vuihandler(void);
void blind_disable_vuihandler(void);

void blind_init(void);
void blind_init_motor(void);

void outdoor_merge_info(outdoor_info_t *incoming_info);

void blind_action_pre_activate(void);
void blind_action_post_activate(void);

void blind_dump(void);

#endif /* BLIND_H */
