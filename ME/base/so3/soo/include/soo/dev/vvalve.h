/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VVALVE_H
#define VVALVE_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#define CMD_VALVE_SIZE 1

#define VVALVE_NAME		"vvalve"
#define VVALVE_PREFIX		"[" VVALVE_NAME "] "

#define VALVE_ACTION_ASK_ID		1
#define VALVE_ACTION_CMD_VALVE	0

#define VALVE_CMD_OPEN		1
#define VALVE_CMD_CLOSE		0



/**
 * cmd:
 * 	1 -> open valve
 *  0 -> close valve
 * action:
 * 	1 -> ask id
 * 	0 -> open/close valve
 **/
typedef struct {
	uint8_t action;
	uint8_t cmd_valve;
} vvalve_request_t;

typedef struct  {
	uint32_t dev_id;
} vvalve_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vvalve, vvalve_request_t, vvalve_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	vvalve_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vvalve_t;

static inline vvalve_t *to_vvalve(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, vvalve_t, vdevfront);
}

void vvalve_generate_request(char *buffer);
int vvalve_get_id(void);
void vvalve_send_cmd(uint8_t cmd);

#endif /* VVALVE_H */
