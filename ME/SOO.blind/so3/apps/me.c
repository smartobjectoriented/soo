/*
 * Copyright (C) 2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
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

#include <mutex.h>
#include <delay.h>
#include <timer.h>
#include <heap.h>
#include <memory.h>

#include <device/irq.h>

#include <soo/avz.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/logbool.h>

#include <soo/dev/vuihandler.h>
#include <soo/dev/vdoga12v6nm.h>

#include <apps/blind.h>

/* SPID of the SOO.blind ME */
uint8_t SOO_blind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x11, 0x8d };

/* Null agency UID to check if an agency UID is valid */
agencyUID_t null_agencyUID = {
	.id = { 0 }
};

/* My agency UID */
agencyUID_t my_agencyUID = {
	.id = { 0 }
};

/* Bool telling that at least 1 post-activate has been performed */
bool post_activate_done = false;

struct completion compl;
mutex_t lock1, lock2;

extern void *localinfo_data;

/*
 * - Global SOO.blind data
 * - Temporary SOO.blind info buffer for SOO presence operations
 * - Protection spinlock
 */
blind_data_t *blind_data;
blind_info_t *tmp_blind_info;
spinlock_t blind_lock;

/* ID of the Smart Object on which the ME is running. 0xff means that it has not been initialized yet. */
uint32_t my_id = 0xff;

/* Agency UID of the Smart Object on which the Entity has been injected */
agencyUID_t origin_agencyUID;

/* Name of the Smart Object on which the Entity has been injected */
char origin_soo_name[SOO_NAME_SIZE];

/* Agency UID of the Smart Object on which the Entity has migrated */
agencyUID_t target_agencyUID;

/* Name of the Smart Object on which the Entity has migrated */
char target_soo_name[SOO_NAME_SIZE];

/* Bool telling that the ME is in a Smart Object with the expected devcaps */
bool available_devcaps = false;

/* Bool telling that a remote application is connected */
bool remote_app_connected = false;

/* Detection of a ME that has migrated on another new Smart Object */
bool has_migrated = false;

/* VUIHandler packet */
static blind_vuihandler_pkt_t *outgoing_vuihandler_pkt;

/* Command deferring processing */
static blind_cmd_t blind_cmds[MAX_BUF_CMDS];
static uint32_t blind_cmd_prod = 0;
static uint32_t blind_cmd_cons = 0;
static struct completion cmd_completion;
static bool cmd_pending = false;

/* Timer used to stop the blind automatically */
static timer_t blind_timer;

/* Automatic calibration at the first user's position set request */
static calibration_step_t calibration_step = NO_CALIB;
/* Tell if a set position request is pending. If this is -1, there is none. */
static int pending_pos = -1;

/***** Command processing and interactions with the motor *****/

/**
 * Compute and update the real position of the blind, that is, its physical position.
 */
static void update_blind_pos(uint32_t id, uint64_t travel_start, uint64_t travel_stop) {
	uint8_t real_blind_position;

	DBG("Update blind position %d\n", id);

	if (blind_data->info.blind[id].real_blind_state == GOING_UP) {
		/* Blind going up: check if we do not exceed bounds */
		if (blind_data->info.blind[id].traveled_time + travel_stop - travel_start > blind_data->info.blind[id].total_time) {
			blind_data->info.blind[id].traveled_time = blind_data->info.blind[id].total_time;
			blind_data->info.blind[id].real_blind_position = FULLY_OPEN_POS;
		} else
			blind_data->info.blind[id].traveled_time += travel_stop - travel_start;
	} else if (blind_data->info.blind[id].real_blind_state == GOING_DOWN) {
		/* Blind going up: check if we do not exceed bounds */
		if (blind_data->info.blind[id].traveled_time < travel_stop - travel_start) {
			blind_data->info.blind[id].traveled_time = 0;
			blind_data->info.blind[id].real_blind_position = FULLY_CLOSED_POS;
		} else
			blind_data->info.blind[id].traveled_time -= travel_stop - travel_start;
	}

	/* Compute the new real blind position using timestamps */
	real_blind_position = (uint8_t) ((blind_data->info.blind[id].traveled_time * (FULLY_OPEN_POS - FULLY_CLOSED_POS) / blind_data->info.blind[id].total_time));

	/*
	 * We accept a margin of POS_MARGIN. If the interval between the newly computed real blind position
	 * and the requested one is below this margin, we consider that there has been a slight computation
	 * error due to round operation. In this case, the real blind position is set to the requested one to
	 * absorb the "error".
	 */
	if (((blind_data->info.blind[id].requested_blind_position > real_blind_position) && (blind_data->info.blind[id].requested_blind_position - real_blind_position < POS_MARGIN)) ||
		((blind_data->info.blind[id].requested_blind_position < real_blind_position) && (real_blind_position - blind_data->info.blind[id].requested_blind_position < POS_MARGIN))) {
		DBG("Adjust real blind position: %d -> %d\n", real_blind_position, blind_data->info.blind[id].requested_blind_position);
		real_blind_position = blind_data->info.blind[id].requested_blind_position;
	}
	blind_data->info.blind[id].real_blind_position = real_blind_position;
	DBG("Real blind position: %d\n", blind_data->info.blind[id].real_blind_position);

	DBG0("travel_time="); lprintk_int64(blind_data->info.blind[id].traveled_time);

	/*
	 * We (re-)set the requested position to the same value than the real position. This operation
	 * is mandatory to handle the case of a "blind up" or "blind down" manual command, which does not
	 * set the requested blind position by itself.
	 * - In the case of a set position request, this operation does not change the requested blind position.
	 * - In the case of a blind up or blind down request, this operation sets the requested blind position to
	 *   the matching position (FULLY_CLOSED_POS for "blind down", FULLY_OPEN_POS for "blind up").
	 */
	blind_data->info.blind[id].requested_blind_position = blind_data->info.blind[id].real_blind_position;
}

/**
 * Add a command to the command buffer.
 * The spinlock protection must be performed by the caller.
 */
static void add_buf_cmd(uint32_t cmd, uint32_t arg) {
	blind_cmds[blind_cmd_prod % MAX_BUF_CMDS].cmd = cmd;
	blind_cmds[blind_cmd_prod % MAX_BUF_CMDS].arg = arg;
	blind_cmd_prod++;
}

/**
 * Process a command.
 * This function should not be called in interrupt context.
 * It should be avoided to call this function in a recursive way. It should be called by setting blind_cmd attributes
 * then calling complete on the cmd_completion.
 */
static void process_cmd(uint32_t cmd, uint32_t arg) {
	uint32_t id, val;
	static uint64_t calibration_start, calibration_stop;
	static uint64_t travel_start, travel_stop;
	static uint64_t travel_requested;
	unsigned long flags;
	uint8_t tmp_pending_pos;

	DBG("0x%08x, 0x%08x\n", cmd, arg);

	switch (cmd) {
	/* Make the blind go up */
	case IOCTL_BLIND_UP:
		DBG("cmd = IOCTL_BLIND_UP\n");

		/*
		 * 0x0000xxyy
		 * xx: 1: do not program automatic blind stop timer, 0: program automatic blind stop timer
		 *     This is usually set to 1 when a set position request has been performed.
		 * yy: index of the Smart Object
		 */
		id = arg & 0xff;
		val = (arg & 0xff00) >> 8;

		/* The target Smart Object is this one */
		if (id == my_id) {
			DBG0("Target!\n");

			spin_lock_irqsave(&blind_lock, flags);

			/* If the blind is already moving, ignore the command */
			if ((blind_data->info.blind[id].real_blind_state != STOPPED) && (blind_data->info.blind[id].real_blind_state != PROCESSING)) {
				DBG0("Blind already moving. Ignore\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			/* If the blind is already up, ignore the command */
			if ((blind_data->info.blind[id].calibration_done) && (blind_data->info.blind[id].real_blind_position == FULLY_OPEN_POS)) {
				DBG0("Blind already fully open. Ignore.\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			/* The blind is now going up */
			blind_data->info.blind[id].requested_blind_state = GOING_UP;
			blind_data->info.blind[id].real_blind_state = GOING_UP;
			blind_data->info.presence[id].change_count++;

			spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
			/* Motor command */
			vdoga12v6nm_set_rotation_direction(1);
			vdoga12v6nm_enable();
#endif /* !NO_MOTOR */
		/* The target Smart Object is not this one */
		} else {
			spin_lock_irqsave(&blind_lock, flags);
			blind_data->info.blind[id].requested_blind_state = GOING_UP;
			blind_data->info.blind[id].real_blind_state = PROCESSING;
			blind_data->info.presence[id].change_count += 10;
			spin_unlock_irqrestore(&blind_lock, flags);
		}

		break;

	/* Make the blind go down */
	case IOCTL_BLIND_DOWN:
		DBG0("cmd = IOCTL_BLIND_DOWN\n");

		/*
		 * 0x0000xxyy
		 * xx: 1: do not program automatic blind stop timer, 0: program automatic blind stop timer
		 * yy: index of the Smart Object
		 */
		id = arg & 0xff;
		val = (arg & 0xff00) >> 8;

		/* The target Smart Object is this one */
		if (id == my_id) {
			DBG0("Target!\n");

			spin_lock_irqsave(&blind_lock, flags);

			/* If the blind is already moving, ignore the command */
			if ((blind_data->info.blind[id].real_blind_state != STOPPED) && (blind_data->info.blind[id].real_blind_state != PROCESSING)) {
				DBG0("Blind already moving. Ignore\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			/* If the blind is already down, ignore the command */
			if ((blind_data->info.blind[id].calibration_done) && (blind_data->info.blind[id].real_blind_position == FULLY_CLOSED_POS)) {
				DBG0("Blind already fully closed. Ignore.\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			/* The blind is now going down */
			blind_data->info.blind[id].requested_blind_state = GOING_DOWN;
			blind_data->info.blind[id].real_blind_state = GOING_DOWN;
			blind_data->info.presence[id].change_count++;

			spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
			/* Motor command */
			vdoga12v6nm_set_rotation_direction(0);
			vdoga12v6nm_enable();
#endif /* !NO_MOTOR */
		/* The target Smart Object is not this one */
		} else {
			spin_lock_irqsave(&blind_lock, flags);
			blind_data->info.blind[id].requested_blind_state = GOING_DOWN;
			blind_data->info.blind[id].real_blind_state = PROCESSING;
			blind_data->info.presence[id].change_count += 10;
			spin_unlock_irqrestore(&blind_lock, flags);
		}

		break;

	/* Stop the blind */
	case IOCTL_BLIND_STOP:
		DBG0("cmd = IOCTL_BLIND_STOP\n");

		/*
		 * 0x0000xxyy
		 * xx: 1: STOP coming from a mechanical stop. 0 otherwise
		 * yy: index of the Smart Object
		 */
		id = arg & 0xff;
		val = (arg & 0xff00) >> 8;

		/* The target Smart Object is this one */
		if (id == my_id) {
			DBG0("Target!\n");

			/* Stop the timer if active */
			stop_timer(&blind_timer);

			spin_lock_irqsave(&blind_lock, flags);

			/* If the blind is not moving, ignore the command */
			if (blind_data->info.blind[id].real_blind_state == STOPPED) {
				DBG0("Blind already stopped. Ignore\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			travel_stop = NOW();
#if defined(DEBUG)
			DBG0("travel_stop="); lprintk_int64(travel_stop);
#endif /* DEBUG */

			/* Calibration is in progress */
			if (blind_data->info.blind[id].doing_calibration) {
				/* The blind is going up */
				if (calibration_step == CALIB_UP) {
					/* Compute total travel time */
					calibration_stop = travel_stop;

					blind_data->info.blind[id].total_time = calibration_stop - calibration_start;
#if defined(DEBUG)
					DBG0("calibration_stop="); lprintk_int64(calibration_stop);
					DBG0("total_time="); lprintk_int64(blind_data->info.blind[id].total_time);
#endif /* DEBUG */

					blind_data->info.blind[id].traveled_time = blind_data->info.blind[id].total_time;
					blind_data->info.blind[id].real_blind_position = FULLY_OPEN_POS;

#if defined(DEBUG)
					DBG0("travel_time="); lprintk_int64(blind_data->info.blind[id].traveled_time);
#endif /* DEBUG */

					DBG0("CALIB UP > NO_CALIB\n");
					calibration_step = NO_CALIB;
					blind_data->info.blind[id].requested_blind_state = STOPPED;
					blind_data->info.blind[id].real_blind_state = STOPPED;
					blind_data->info.presence[id].change_count++;

					/* If there is a pending set position request, process it */
					if (pending_pos != -1) {
						tmp_pending_pos = (uint8_t) pending_pos;
						pending_pos = -1;

						add_buf_cmd(IOCTL_STOP_CALIBRATION, id & 0xff);
						add_buf_cmd(IOCTL_SET_POS, ((id & 0xff) << 8) | (tmp_pending_pos & 0xff));

						spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
						vdoga12v6nm_disable();
#endif /* !NO_MOTOR */

						DBG("Pending position: %d\n", tmp_pending_pos);
						complete(&cmd_completion);

						break;
					/* There is no pending set position request, the calibration is now over */
					} else {
						DBG0("No pending position\n");

						add_buf_cmd(IOCTL_STOP_CALIBRATION, id & 0xff);

						spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
						vdoga12v6nm_disable();
#endif /* !NO_MOTOR */

						complete(&cmd_completion);

						break;
					}
				/* The blind is going down */
				} else if (calibration_step == CALIB_DOWN) {
					DBG0("CALIB_DOWN > CALIB_UP\n");
					calibration_step = CALIB_UP;
					blind_data->info.blind[id].requested_blind_state = STOPPED;
					blind_data->info.blind[id].real_blind_state = STOPPED;
					blind_data->info.presence[id].change_count++;

					travel_start = NOW();
#if defined(DEBUG)
					DBG0("travel_start="); lprintk_int64(travel_start);
#endif /* DEBUG */
					calibration_start = travel_start;
					DBG0("calibration_start="); lprintk_int64(calibration_start);

					/* (1 << 8): avoid timer to be reprogrammed during the operation */
					add_buf_cmd(IOCTL_BLIND_UP, (1 << 8) | (id & 0xff));

					spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
					vdoga12v6nm_disable();
#endif /* !NO_MOTOR */

					complete(&cmd_completion);

					break;
				/* The desired position has been reached and the calibration is now over */
				} else {
					update_blind_pos(id, travel_start, travel_stop);

					blind_data->info.blind[id].requested_blind_state = STOPPED;
					blind_data->info.blind[id].real_blind_state = STOPPED;
					blind_data->info.presence[id].change_count++;

					add_buf_cmd(IOCTL_STOP_CALIBRATION, id & 0xff);

					spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
					vdoga12v6nm_disable();
#endif /* !NO_MOTOR */

					complete(&cmd_completion);

					break;
				}
			/* Calibration has been performed */
			} else if (blind_data->info.blind[id].calibration_done) {
				if (!val)
					update_blind_pos(id, travel_start, travel_stop);
			}

			blind_data->info.blind[id].requested_blind_state = STOPPED;
			blind_data->info.blind[id].real_blind_state = STOPPED;
			blind_data->info.presence[id].change_count++;

			spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
			vdoga12v6nm_disable();
#endif /* !NO_MOTOR */
		/* The target Smart Object is not this one */
		} else {
			spin_lock_irqsave(&blind_lock, flags);
			blind_data->info.blind[id].requested_blind_state = STOPPED;
			blind_data->info.blind[id].real_blind_state = PROCESSING;
			blind_data->info.presence[id].change_count += 10;
			spin_unlock_irqrestore(&blind_lock, flags);
		}

		break;

	/* Start the calibration */
	case IOCTL_START_CALIBRATION:
		DBG0("cmd = IOCTL_START_CALIBRATION\n");

		/*
		 * 0x000000xx
		 * xx: index of the Smart Object
		 */
		id = arg & 0xff;

		/*
		 * Calibration is intended to be performed only on the Smart Object to which the tablet is connected. This check is done
		 * by the remote application.
		 */
		if (id == my_id) {
			DBG0("Target!\n");

			spin_lock_irqsave(&blind_lock, flags);

			/* Calibration can be started when the blind is stopped */
			if ((((blind_data->info.blind[id].real_blind_state != STOPPED) && (blind_data->info.blind[id].real_blind_state != PROCESSING))) || (calibration_step != NO_CALIB)) {
				DBG("Cannot start calibration. Real blind state: %d, calibration step: %d\n", blind_data->info.blind[id].real_blind_state, calibration_step);

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			blind_data->info.blind[id].doing_calibration = true;
			blind_data->info.blind[id].calibration_done = false;
			blind_data->info.blind[id].traveled_time = 0;

			DBG0("NO_CALIB > CALIB_DOWN\n");
			calibration_step = CALIB_DOWN;
			blind_data->info.blind[id].requested_blind_state = STOPPED;
			blind_data->info.blind[id].real_blind_state = STOPPED;
			blind_data->info.presence[id].change_count++;

			/* (1 << 8): avoid timer to be reprogramed during the operation */
			add_buf_cmd(IOCTL_BLIND_DOWN, (1 << 8) | (id & 0xff));

			spin_unlock_irqrestore(&blind_lock, flags);

#ifndef NO_MOTOR
			/* Be sure that the motor is off! */
			vdoga12v6nm_disable();
#endif /* !NO_MOTOR */

			complete(&cmd_completion);
		}

		break;

	/* Stop the calibration */
	case IOCTL_STOP_CALIBRATION:
		DBG0("cmd = IOCTL_STOP_CALIBRATION\n");

		/*
		 * 0x000000xx
		 * xx: index of the Smart Object
		 */
		id = arg & 0xff;

		if (id == my_id) {
			DBG0("Target!\n");

			spin_lock_irqsave(&blind_lock, flags);

			blind_data->info.blind[id].doing_calibration = false;
			blind_data->info.blind[id].calibration_done = true;
			blind_data->info.presence[id].change_count++;

			spin_unlock_irqrestore(&blind_lock, flags);
		}

		break;

	/* Set the low light intensity threshold */
	case IOCTL_SET_LOW_LIGHT:
		DBG0("cmd = IOCTL_SET_LOW_LIGHT\n");

		/*
		 * 0x00xxyyyy
		 * xx: index of the Smart Object
		 * yyyy: requested low light threshold
		 */
		id = (arg & 0xff0000) >> 16;
		val = arg & 0xffff;

		spin_lock_irqsave(&blind_lock, flags);
		blind_data->info.blind[id].low_light = val;
		blind_data->info.presence[id].change_count += 10;
		spin_unlock_irqrestore(&blind_lock, flags);

		break;

	/* Set the strong light intensity threshold */
	case IOCTL_SET_STRONG_LIGHT:
		DBG0("cmd = IOCTL_SET_STRONG_LIGHT\n");

		/*
		 * 0x00xxyyyy
		 * xx: index of the Smart Object
		 * yyyy: requested strong light threshold
		 */
		id = (arg & 0xff0000) >> 16;
		val = arg & 0xffff;

		spin_lock_irqsave(&blind_lock, flags);
		blind_data->info.blind[id].strong_light = val;
		blind_data->info.presence[id].change_count += 10;
		spin_unlock_irqrestore(&blind_lock, flags);

		break;

	/* Set the strong wind threshold */
	case IOCTL_SET_STRONG_WIND:
		DBG0("cmd = IOCTL_SET_STRONG_WIND\n");

		/*
		 * 0x00xxyyyy
		 * xx: index of the Smart Object
		 * yyyy: requested strong wind threshold
		 */
		id = (arg & 0xff0000) >> 16;
		val = arg & 0xffff;

		spin_lock_irqsave(&blind_lock, flags);
		blind_data->info.blind[id].strong_wind = val;
		blind_data->info.presence[id].change_count += 10;
		spin_unlock_irqrestore(&blind_lock, flags);

		break;

	/* Set desired blind position */
	case IOCTL_SET_POS:
		DBG0("cmd = IOCTL_SET_POS\n");

		/*
		 * 0x0000xxyy
		 * xx: index of the Smart Object
		 * yy: requested position
		 */
		id = (arg & 0xff00) >> 8;
		val = arg & 0xff;

		/* The target Smart Object is this one */
		if (id == my_id) {
			DBG0("Target!\n");

			spin_lock_irqsave(&blind_lock, flags);

			/* If the blind is already moving, ignore the command */
			if ((blind_data->info.blind[id].real_blind_state != STOPPED) && (blind_data->info.blind[id].real_blind_state != PROCESSING)) {
				DBG0("Blind already moving. Ignore\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			/* If the calibration is already in progress, ignore the command */
			if (blind_data->info.blind[id].doing_calibration) {
				DBG0("Calibration in progress. Ignore\n");

				spin_unlock_irqrestore(&blind_lock, flags);
				break;
			}

			travel_start = NOW();
#if defined(DEBUG)
			DBG0("travel_start="); lprintk_int64(travel_start);
#endif /* DEBUG */

			/* Calibration must have been performed */
			if (blind_data->info.blind[id].calibration_done) {
				blind_data->info.blind[id].requested_blind_position = val;

				/* If the requested position is 100%, make the blind go up until it reaches the mechanical stop */
				if (blind_data->info.blind[id].requested_blind_position == FULLY_OPEN_POS) {
					if (blind_data->info.blind[id].real_blind_position == FULLY_OPEN_POS) {
						DBG0("Blind already fully open. Ignore.\n");

						spin_unlock_irqrestore(&blind_lock, flags);
						break;
					}

					add_buf_cmd(IOCTL_BLIND_UP, (1 << 8) | (id & 0xff));

					spin_unlock_irqrestore(&blind_lock, flags);

					complete(&cmd_completion);

					break;
				}

				/* If the requested position is 0%, make the blind go down until it reaches the mechanical stop */
				if (blind_data->info.blind[id].requested_blind_position == FULLY_CLOSED_POS) {
					if (blind_data->info.blind[id].real_blind_position == FULLY_CLOSED_POS) {
						DBG0("Blind already fully closed. Ignore.\n");

						spin_unlock_irqrestore(&blind_lock, flags);
						break;
					}

					add_buf_cmd(IOCTL_BLIND_DOWN, (1 << 8) | (id & 0xff));

					spin_unlock_irqrestore(&blind_lock, flags);

					complete(&cmd_completion);

					break;
				}

				/* The requested position is below the current one */
				if (blind_data->info.blind[id].requested_blind_position < blind_data->info.blind[id].real_blind_position) {
					DBG("Blind down: %d -> %d\n", blind_data->info.blind[id].real_blind_position, blind_data->info.blind[id].requested_blind_position);

					/* Compute remaining time for the blind to reach the desired position */
					travel_requested = blind_data->info.blind[id].total_time *
								(blind_data->info.blind[id].real_blind_position - blind_data->info.blind[id].requested_blind_position) /
								(FULLY_OPEN_POS - FULLY_CLOSED_POS);
#if defined(DEBUG)
					DBG0("Set timer: "); lprintk_int64(travel_requested);
#endif /* DEBUG */
					/* Set timer to stop the blind when it reaches the desired position */
					set_timer(&blind_timer, travel_start + travel_requested);

					/* (1 << 8): avoid timer to be reprogrammed during the operation */
					add_buf_cmd(IOCTL_BLIND_DOWN, (1 << 8) | (id & 0xff));

					blind_data->info.presence[id].change_count += 10;

					spin_unlock_irqrestore(&blind_lock, flags);

					complete(&cmd_completion);
				/* The requested position is above the current one */
				} else if (blind_data->info.blind[id].requested_blind_position > blind_data->info.blind[id].real_blind_position) {
					DBG("Blind up: %d -> %d\n", blind_data->info.blind[id].real_blind_position, blind_data->info.blind[id].requested_blind_position);

					/* Compute remaining time for the blind to reach the desired position */
					travel_requested = blind_data->info.blind[id].total_time *
								(blind_data->info.blind[id].requested_blind_position - blind_data->info.blind[id].real_blind_position) /
								(FULLY_OPEN_POS - FULLY_CLOSED_POS);
#if defined(DEBUG)
					DBG0("Set timer: "); lprintk_int64(travel_requested);
#endif /* DEBUG */
					/* Set timer to stop the blind when it reaches the desired position */
					set_timer(&blind_timer, travel_start + travel_requested);

					/* (1 << 8): avoid timer to be reprogrammed during the operation */
					add_buf_cmd(IOCTL_BLIND_UP, (1 << 8) | (id & 0xff));

					blind_data->info.presence[id].change_count += 10;

					spin_unlock_irqrestore(&blind_lock, flags);

					complete(&cmd_completion);
				} else
					spin_unlock_irqrestore(&blind_lock, flags);
			} else {
				/* Save the position request value */
				pending_pos = val;

				add_buf_cmd(IOCTL_START_CALIBRATION, id);

				spin_unlock_irqrestore(&blind_lock, flags);

				/* Launch the automatic calibration */
				complete(&cmd_completion);
			}
		/* The target Smart Object is not this one */
		} else {
			spin_lock_irqsave(&blind_lock, flags);
			blind_data->info.blind[id].real_blind_state = PROCESSING;
			blind_data->info.blind[id].requested_blind_position = val;
			blind_data->info.presence[id].change_count += 10;
			spin_unlock_irqrestore(&blind_lock, flags);
		}

		break;
	}

	DBG0("Process cmd OK\n");
}

/**
 * Reset a descriptor associated to a SOO.blind Smart Object.
 */
static void reset_blind_desc(uint32_t index) {
	blind_data->info.presence[index].active = false;
	memset(blind_data->info.presence[index].name, 0, MAX_NAME_SIZE);
	memset(blind_data->info.presence[index].agencyUID, 0, SOO_AGENCY_UID_SIZE);
	blind_data->info.presence[index].change_count = 0;
	blind_data->info.presence[index].age = 0;
	blind_data->info.presence[index].last_age = 0;
	blind_data->info.presence[index].inertia = 0;

	blind_data->info.blind[index].requested_blind_position = 0;
	blind_data->info.blind[index].real_blind_position = 0;
	blind_data->info.blind[index].requested_blind_state = STOPPED;
	blind_data->info.blind[index].real_blind_state = STOPPED;
	blind_data->info.blind[index].calibration_done = false;
	blind_data->info.blind[index].doing_calibration = false;
	blind_data->info.blind[index].total_time = 0;
	blind_data->info.blind[index].traveled_time = 0;

	blind_data->info.blind[index].low_light = 0;
	blind_data->info.blind[index].strong_light = 0;
	blind_data->info.blind[index].strong_wind = 0;
}

/***** Interactions with the SOO Presence Pattern *****/

/**
 * Reset a descriptor.
 */
void reset_desc(void *local_info_ptr, uint32_t index) {
	reset_blind_desc(index);
}

/**
 * Copy a descriptor.
 */
void copy_desc(void *dest_info_ptr, uint32_t local_index, void *src_info_ptr, uint32_t remote_index) {
	blind_info_t *src_info = (blind_info_t *) src_info_ptr;

	memcpy(&blind_data->info.blind[local_index], &src_info->blind[remote_index], sizeof(blind_desc_t));
	memcpy(&blind_data->info.presence[local_index], &src_info->presence[remote_index], sizeof(soo_presence_data_t));
}

/**
 * Copy the information of an incoming SOO.* descriptor into the local global SOO.* info.
 * The whole descriptor is overwritten.
 */
void update_desc(uint8_t *agencyUID,
			void *local_info_ptr, uint32_t local_index,
			void *recv_data_ptr, uint32_t remote_index) {
	blind_info_t *incoming_info = (blind_info_t *) recv_data_ptr;

	memcpy(&blind_data->info.blind[local_index], &incoming_info->blind[remote_index], sizeof(blind_desc_t));
	memcpy(&blind_data->info.presence[local_index], &incoming_info->presence[remote_index], sizeof(soo_presence_data_t));
}

/**
 * If, after the merge, a SOO.blind update is necessary, bufferize the request so that the command will
 * be executed at the next post-activate.
 * This function is executed in domcall context. No call to completion() should be made here.
 */
void action_after_merge(void *local_info_ptr, uint32_t local_index, uint32_t remote_index) {
	if (local_index == my_id) {
		DBG("calibration_done=%d, doing_calibration=%d, requested_blind_state=%d, real_blind_state=%d, requested_blind_position=%d, real_blind_position=%d\n",
			blind_data->info.blind[local_index].calibration_done,
			blind_data->info.blind[local_index].doing_calibration,
			blind_data->info.blind[local_index].requested_blind_state,
			blind_data->info.blind[local_index].real_blind_state,
			blind_data->info.blind[local_index].requested_blind_position,
			blind_data->info.blind[local_index].real_blind_position);

		/* Up, down or stop blind request */
		if ((blind_data->info.blind[local_index].calibration_done) &&
			(blind_data->info.blind[local_index].requested_blind_state != blind_data->info.blind[local_index].real_blind_state)) {

			switch (blind_data->info.blind[local_index].requested_blind_state) {
			case STOPPED:
				add_buf_cmd(IOCTL_BLIND_STOP, local_index);
				break;

			case GOING_UP:
				add_buf_cmd(IOCTL_BLIND_UP, local_index);
				break;

			case GOING_DOWN:
				add_buf_cmd(IOCTL_BLIND_DOWN, local_index);
				break;

			case PROCESSING:
				/* Not used */
				break;
			}

			/* As we are in domcall context, defer command processing */
			cmd_pending = true;
		}

		/* Set position request */
		if ((!blind_data->info.blind[local_index].doing_calibration) &&
			(blind_data->info.blind[local_index].requested_blind_position != blind_data->info.blind[local_index].real_blind_position)) {
			/* Set position request */

			add_buf_cmd(IOCTL_SET_POS, ((local_index & 0xff) << 8) | (blind_data->info.blind[local_index].requested_blind_position & 0xff));

			/* As we are in domcall context, defer command processing */
			cmd_pending = true;
		}
	}
}

/**
 * Update the ID associated to this Smart Object in the SOO.blind info.
 */
void update_my_id(uint32_t new_id) {
	blind_data->info.my_id = my_id;
}

/***** Callbacks *****/

void blind_action_pre_activate(void) {
	/* Nothing to do */
}

/**
 * In post-activate, process the bufferized command if any.
 */
void blind_action_post_activate(void) {
	if (cmd_pending) {
		cmd_pending = false;

		DBG("Command pending\n");

		/* Defer command processing as it can interact with a frontend */
		complete(&cmd_completion);
	}
}

/***** Interactions with the vUIHandler interface *****/

/**
 * Function called when the connected application ME SPID changes.
 * - If the ME is running on a SOO.blind Smart Object: the ME stays resident.
 * - If the ME is on a non-SOO.blind Smart Object: the ME must stay resident as long as the remote
 *   application is connected.
 */
void ui_update_app_spid(uint8_t *spid) {
	agency_ctl_args_t agency_ctl_args;

#ifdef DEBUG
	DBG("App ME SPID change: ");
	lprintk_buffer(spid, SPID_SIZE);
#endif /* DEBUG */

	if ((!available_devcaps) && (memcmp(spid, SOO_blind_spid, SPID_SIZE)) != 0) {
		DBG("Force Terminate ME in slot ID: %d\n", ME_domID() + 1);
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID() + 1;
		agency_ctl(&agency_ctl_args);
	}
}

/**
 * Interface with vUIHandler. This function is called when a VUIHandler packet is received from the application
 * on the tablet/smartphone.
 */
void ui_interrupt(char *data, size_t size) {
	blind_cmd_t tmp_blind_cmd;

	spin_lock(&blind_lock);

	memcpy(&tmp_blind_cmd, data, sizeof(blind_cmd_t));
	DBG("0x%08x, 0x%08x\n", tmp_blind_cmd.cmd, tmp_blind_cmd.arg);

	add_buf_cmd(tmp_blind_cmd.cmd, tmp_blind_cmd.arg);

	spin_unlock(&blind_lock);

	/* Defer command processing as it can interact with a frontend */
	complete(&cmd_completion);
}

/**
 * Interface with vDoga12V6Nm. This function is called when the blind reaches the up mechanical stop,
 * meaning that the blind is completely up.
 */
void up_interrupt(void) {
	DBG0("Up interrupt\n");

	spin_lock(&blind_lock);

	if (blind_data->info.blind[my_id].real_blind_state == GOING_DOWN) {
		DBG("Blind going down (%d) but up mechanical stop triggered. Ignore\n");

		spin_unlock(&blind_lock);
		return ;
	}

	/* Update blind data */
	blind_data->info.blind[my_id].real_blind_position = FULLY_OPEN_POS;
	if (blind_data->info.blind[my_id].calibration_done) {
		blind_data->info.blind[my_id].traveled_time = blind_data->info.blind[my_id].total_time;
		blind_data->info.blind[my_id].requested_blind_position = blind_data->info.blind[my_id].real_blind_position;
	}

	DBG("Update blind position %d\n", my_id);
	DBG("Real blind position: %d\n", blind_data->info.blind[my_id].real_blind_position);

	/* (1 << 8): avoid updating travel time, as it has been performed here */
	add_buf_cmd(IOCTL_BLIND_STOP, (1 << 8) | (my_id & 0xff));

	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}

/**
 * Interface with vDoga12V6Nm. This function is called when the blind reaches the down mechanical stop,
 * meaning that the blind is completely down.
 */
void down_interrupt(void) {
	DBG0("Down interrupt\n");

	spin_lock(&blind_lock);

	if (blind_data->info.blind[my_id].real_blind_state == GOING_UP) {
		DBG("Blind going up (%d) but down mechanical stop triggered. Ignore\n");

		spin_unlock(&blind_lock);
		return ;
	}

	/* Update blind data */
	blind_data->info.blind[my_id].real_blind_position = FULLY_CLOSED_POS;
	if (blind_data->info.blind[my_id].calibration_done) {
		blind_data->info.blind[my_id].traveled_time = 0;
		blind_data->info.blind[my_id].requested_blind_position = blind_data->info.blind[my_id].real_blind_position;
	}

	DBG("Update blind position %d\n", my_id);
	DBG("Real blind position: %d\n", blind_data->info.blind[my_id].real_blind_position);

	/* (1 << 8): avoid updating travel time, as it has been performed here */
	add_buf_cmd(IOCTL_BLIND_STOP, (1 << 8) | (my_id & 0xff));

	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}

/**
 * Timer handle function executed when the timer expires.
 * The expiry of the timer occurs when the blind is completely up or down, thus it needs to be stopped.
 */
static void blind_timer_fn(void *data) {
	unsigned long flags;

	DBG0("Timer timeout\n");

	if (__in_interrupt)
		spin_lock(&blind_lock);
	else
		spin_lock_irqsave(&blind_lock, flags);

	add_buf_cmd(IOCTL_BLIND_STOP, my_id);

	if (__in_interrupt)
		spin_unlock(&blind_lock);
	else
		spin_unlock_irqrestore(&blind_lock, flags);

	complete(&cmd_completion);
}

/***** Tasks *****/

/**
 * Task that sends a VUIHandler packet with the SOO.blind info periodically.
 */
static int send_vuihandler_pkt_task_fn(void *arg) {
	unsigned long flags;

	while (1) {
		msleep(VUIHANDLER_PERIOD);

		spin_lock_irqsave(&blind_lock, flags);
		memcpy(outgoing_vuihandler_pkt->payload, &blind_data->info, sizeof(blind_info_t));
		spin_unlock_irqrestore(&blind_lock, flags);

		vuihandler_send(outgoing_vuihandler_pkt, BLIND_VUIHANDLER_HEADER_SIZE + BLIND_VUIHANDLER_DATA_SIZE);
	}

	return 0;
}

/**
 * Task that processes commands. Commands coming from the user should not be processed in interrupt context.
 * This task processes the commands in a deferred way. They are taken from the command buffer.
 */
static int cmd_task_fn(void *arg) {
	blind_cmd_t tmp_blind_cmd[MAX_BUF_CMDS];
	unsigned long flags;
	uint32_t i;
	uint32_t cons, prod;

	while (1) {
		wait_for_completion(&cmd_completion);

		spin_lock_irqsave(&blind_lock, flags);
		memcpy(&tmp_blind_cmd, &blind_cmds, sizeof(blind_cmds));
		cons = blind_cmd_cons;
		prod = blind_cmd_prod;
		blind_cmd_cons = blind_cmd_prod;
		spin_unlock_irqrestore(&blind_lock, flags);

		for (i = cons; i < prod; i++)
			process_cmd(tmp_blind_cmd[i % MAX_BUF_CMDS].cmd, tmp_blind_cmd[i % MAX_BUF_CMDS].arg);

		/*
		 * Send the updated global indoor info as reply.
		 * This will allow the tablet app to update its display as quickly as possible.
		 */
		spin_lock_irqsave(&blind_lock, flags);
		memcpy(outgoing_vuihandler_pkt->payload, &blind_data->info, sizeof(blind_info_t));
		spin_unlock_irqrestore(&blind_lock, flags);

		vuihandler_send(outgoing_vuihandler_pkt, BLIND_VUIHANDLER_HEADER_SIZE + BLIND_VUIHANDLER_DATA_SIZE);
	}

	return 0;
}

/***** Initialization *****/

/**
 * Start the threads.
 */
void blind_start_threads(void) {
	kernel_thread(send_vuihandler_pkt_task_fn, "VUIHandler", NULL, 0);
	kernel_thread(cmd_task_fn, "cmd", NULL, 0);
}

/**
 * Create the SOO.blind descriptor of this Smart Object.
 */
void create_my_desc(void) {
	reset_blind_desc(0);

	memcpy(blind_data->info.presence[0].agencyUID, &origin_agencyUID, SOO_AGENCY_UID_SIZE);
	strcpy((char *) blind_data->info.presence[0].name, origin_soo_name);
	blind_data->info.presence[0].active = true;

	memcpy(blind_data->origin_agencyUID, &origin_agencyUID, SOO_AGENCY_UID_SIZE);
}

/**
 * Application initialization.
 */
void blind_init(void) {
	uint32_t i;

	/* Data buffer allocation */
	blind_data = (blind_data_t *) localinfo_data;
	memset(blind_data, 0, sizeof(blind_data_t));
	tmp_blind_info = (blind_info_t *) malloc(sizeof(blind_info_t));
	memset(tmp_blind_info, 0, sizeof(blind_info_t));
	spin_lock_init(&blind_lock);

	/* Reset all SOO.blind descriptors */
	for (i = 0; i < MAX_DESC; i++)
		reset_blind_desc(i);

	/* Clear command buffer */
	for (i = 0; i < MAX_BUF_CMDS; i++) {
		blind_cmds[i].cmd = 0;
		blind_cmds[i].arg = 0;
	}

	vdoga12v6nm_register_interrupts(up_interrupt, down_interrupt);
	vuihandler_register_callback(ui_update_app_spid, ui_interrupt);

	/* Allocate the outgoing VUIHandler packet */
	outgoing_vuihandler_pkt = (blind_vuihandler_pkt_t *) malloc(BLIND_VUIHANDLER_HEADER_SIZE + BLIND_VUIHANDLER_DATA_SIZE);

	init_completion(&cmd_completion);

	/* Initialize the blind stop timer */
	init_timer(&blind_timer, &blind_timer_fn, NULL);

	DBG0("SOO." APP_NAME " ME ready\n");
}

/**
 * Configure the motor.
 */
void blind_init_motor(void) {
#ifndef NO_MOTOR
	if (available_devcaps) {
		/* Motor setup: stop, 50% speed */
		vdoga12v6nm_disable();
		vdoga12v6nm_set_percentage_speed(DEFAULT_SPEED);
	}
#endif /* !NO_MOTOR */
}

/***** Synergy with a SOO.outdoor ME *****/

/**
 * Use incoming SOO.outdoor information.
 * This function is executed in domcall context. No call to completion() should be made here.
 */
void outdoor_merge_info(outdoor_info_t *incoming_info) {
	DBG("Merge SOO.outdoor: %dlx, %d x 1E-1m/s\n", incoming_info->outdoor[0].light, incoming_info->outdoor[0].wind);

	/* Down the blind when the low light intensity threshold is reached */
	if ((blind_data->info.blind[my_id].calibration_done) &&
		(blind_data->info.blind[my_id].real_blind_state == STOPPED) &&
		(blind_data->info.blind[my_id].low_light != 0) &&
		(incoming_info->outdoor[0].light <= blind_data->info.blind[my_id].low_light)) {
		if ((blind_data->info.blind[my_id].calibration_done) &&
			(blind_data->info.blind[my_id].real_blind_position != FULLY_CLOSED_POS)) {
			/* Calibration must have been performed */

			DBG0("Low light intensity threshold: blind down\n");

			add_buf_cmd(IOCTL_BLIND_DOWN, my_id);

			/* As we are in domcall context, defer command processing */
			cmd_pending = true;
		}
	}

	/* Up the blind when the up light intensity threshold is reached */
	if ((blind_data->info.blind[my_id].calibration_done) &&
		(blind_data->info.blind[my_id].real_blind_state == STOPPED) &&
		(blind_data->info.blind[my_id].strong_light != 0) &&
		(incoming_info->outdoor[0].light >= blind_data->info.blind[my_id].strong_light)) {
		if ((blind_data->info.blind[my_id].calibration_done) &&
			(blind_data->info.blind[my_id].real_blind_position != FULLY_OPEN_POS)) {
			/* Calibration must have been performed */

			DBG0("Strong light intensity threshold: blind up\n");

			add_buf_cmd(IOCTL_BLIND_UP, my_id);

			/* As we are in domcall context, defer command processing */
			cmd_pending = true;
		}
	}

	/* Up the blind when the wind speed threshold is reached */
	if ((blind_data->info.blind[my_id].calibration_done) &&
		(blind_data->info.blind[my_id].real_blind_state == STOPPED) &&
		(blind_data->info.blind[my_id].strong_wind != 0) &&
		(incoming_info->outdoor[0].wind >= blind_data->info.blind[my_id].strong_wind)) {
		if ((blind_data->info.blind[my_id].calibration_done) &&
			(blind_data->info.blind[my_id].real_blind_position != FULLY_OPEN_POS)) {
			/* Calibration must have been performed */

			DBG0("Strong wind intensity threshold: blind up\n");

			add_buf_cmd(IOCTL_BLIND_UP, my_id);

			/* As we are in domcall context, defer command processing */
			cmd_pending = true;
		}
	}
}

/***** Debugging *****/

/**
 * Dump the contents of the SOO.blind info.
 * For debugging purposes.
 */
void blind_dump(void) {
	uint32_t i;

	spin_lock(&blind_lock);
	memcpy(tmp_blind_info, &blind_data->info, sizeof(blind_info_t));
	spin_unlock(&blind_lock);

	lprintk("My ID: %d\n", my_id);

	for (i = 0; i < MAX_DESC; i++) {
		if (!tmp_blind_info->presence[i].active)
			continue;

		lprintk("%d:\n", i);
		lprintk("    Name: %s\n", tmp_blind_info->presence[i].name);
		lprintk("    Agency UID: "); lprintk_buffer(tmp_blind_info->presence[i].agencyUID, SOO_AGENCY_UID_SIZE);
		lprintk("    Age: %d\n", tmp_blind_info->presence[i].age);
		lprintk("    Inertia: %d\n", tmp_blind_info->presence[i].inertia);
		lprintk("    Change count: %d\n", tmp_blind_info->presence[i].change_count);
		lprintk("    Travel time: "); lprintk_int64(tmp_blind_info->blind[i].traveled_time);
		lprintk("    Total time: "); lprintk_int64(tmp_blind_info->blind[i].total_time);
		lprintk("    Requested blind position: %d\n", tmp_blind_info->blind[i].requested_blind_position);
		lprintk("    Real blind position: %d\n", tmp_blind_info->blind[i].real_blind_position);
		lprintk("    Requested blind status: ");
		switch (tmp_blind_info->blind[i].requested_blind_state) {
		case STOPPED:
			lprintk("stopped\n");
			break;

		case GOING_UP:
			lprintk("going up\n");
			break;

		case GOING_DOWN:
			lprintk("going down\n");
			break;

		case PROCESSING:
			lprintk("processing\n");
			break;
		}
		lprintk("    Real blind status: ");
		switch (tmp_blind_info->blind[i].real_blind_state) {
		case STOPPED:
			lprintk("stopped\n");
			break;

		case GOING_UP:
			lprintk("going up\n");
			break;

		case GOING_DOWN:
			lprintk("going down\n");
			break;

		case PROCESSING:
			lprintk("processing\n");
			break;
		}
		lprintk("    Calibration in progress: %c\n", (tmp_blind_info->blind[i].doing_calibration) ? 'y' : 'n');
		lprintk("    Calibration done: %c\n", (tmp_blind_info->blind[i].calibration_done) ? 'y' : 'n');
		lprintk("    Low light: %d", tmp_blind_info->blind[i].low_light);
		lprintk("    Strong light: %d", tmp_blind_info->blind[i].strong_light);
		lprintk("    Strong wind: %d", tmp_blind_info->blind[i].strong_wind);
		lprintk("\n");
	}
}

#warning Manual blind control for debugging purposes
/*
 * Some keys can be used to control the blind manually. They should be used only for debugging purposes.
 * 0: set position of SOO.blind 0 to 50%.
 * 1: set position of SOO.blind 1 to 50%.
 * 2: set position of SOO.blind 2 to 50%.
 * 9: make the blind go up. This does not update the real blind position in the descriptor.
 * 6: set position of SOO.blind 0 to 50%.
 * 3: make the blind go down. This does not update the real blind position in the descriptor.
 * ?: dump the current SOO.blind descriptors.
 */
void test0(void) {
	lprintk("Set pos 0\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_SET_POS, ((0 & 0xff) << 8) | (50 & 0xff));
	spin_unlock(&blind_lock);

	complete(&cmd_completion);

}
void test1(void) {
	lprintk("Set pos 1\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_SET_POS, ((1 & 0xff) << 8) | (50 & 0xff));
	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}
void test2(void) {
	lprintk("Set pos 2\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_SET_POS, (2 << 8) | (50 & 0xff));
	spin_unlock(&blind_lock);

	complete(&cmd_completion);

}
void blindup(void) {
	lprintk("Up\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_BLIND_UP, my_id);
	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}
void blinddown(void) {
	lprintk("Down\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_BLIND_DOWN, my_id);
	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}
void blindstop(void) {
	lprintk("Stop\n");

	spin_lock(&blind_lock);
	add_buf_cmd(IOCTL_BLIND_STOP, (1 << 8) | my_id);
	spin_unlock(&blind_lock);

	complete(&cmd_completion);
}

/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
int main_kernel(void *args) {
	agency_ctl_args_t agency_ctl_args;

	lprintk("SOO." APP_NAME " Mobile Entity booting ...\n");

	avz_shared_info->dom_desc.u.ME.spad.valid = true;

	soo_guest_activity_init();

	callbacks_init();

	/* Initialize the Vbus subsystem */
	vbus_init();

	gnttab_init();

	vbstore_init_dev_populate();

	agency_ctl_args.cmd = AG_AGENCY_UID;
	if (agency_ctl(&agency_ctl_args) < 0)
		BUG();

	memcpy(&my_agencyUID, &agency_ctl_args.u.agencyUID_args.agencyUID, SOO_AGENCY_UID_SIZE);
	DBG("Agency UID: "); DBG_BUFFER(&my_agencyUID, SOO_AGENCY_UID_SIZE);

	/* Initializing debugging facilities */
	init_dbgevent();
	set_dbgevent_mode(DBGLIST_CONTINUOUS);
	init_dbglist();
	set_dbglist_mode(DBGLIST_CONTINUOUS);


	/* Initialize the application */
	blind_init();

	avz_shared_info->dom_desc.u.ME.spad.valid = true;

	lprintk("SOO." APP_NAME " Mobile Entity\n\n");

	DBG("ME running as domain %d\n", ME_domID());

	return 0;
}
