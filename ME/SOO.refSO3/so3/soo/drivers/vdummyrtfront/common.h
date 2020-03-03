/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <mutex.h>

#include <asm/atomic.h>

#include <device/irq.h>

#include <soo/evtchn.h>
#include <soo/avz.h>

#include <soo/dev/vdummyrt.h>

#ifdef SOOTEST_DRIVERS_DUMMY
#define SOOTEST_N_PREDEF_PACKETS		16
#define SOOTEST_SHA1_HASH_LEN			20
#define SOOTEST_CYCLE_DELAY			100
#define SOOTEST_N_ITER				64
#endif

typedef struct {

	struct vbus_device  *dev;

	vdummyrt_front_ring_t ring;
	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;
	uint32_t irq;

} vdummyrt_t;

extern vdummyrt_t vdummyrt;

irq_return_t vdummyrt_interrupt(int irq, void *data);

void vdummyrt_probe(void);
void vdummyrt_closed(void);
void vdummyrt_suspend(void);
void vdummyrt_resume(void);
void vdummyrt_connected(void);
void vdummyrt_reconfiguring(void);
void vdummyrt_shutdown(void);

void vdummyrt_vbus_init(void);

/* Processing and connected state management */
void vdummyrt_start(void);
void vdummyrt_end(void);
bool vdummyrt_is_connected(void);

