/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/types.h>

#include <asm/string.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

#define N_SLOTS			16
/* This should be a power of 2 to avoid the necessity to use fp/div operations. */
#define N_SAMPLES_PER_SLOT	16

typedef enum {
	DBGLIB_SLOT_FREE = 0,
	DBGLIB_SLOT_BUSY
} dbglib_slot_status_t;

static dbglib_slot_status_t dbglib_slot_status[N_SLOTS];

static s64 dbglib_samples[N_SLOTS][N_SAMPLES_PER_SLOT];
static uint32_t dbglib_count[N_SLOTS];

uint32_t dbglib_reserve_free_slot(void) {
	uint32_t i;

	for (i = 0; i < N_SLOTS; i++) {
		if (dbglib_slot_status[i] == DBGLIB_SLOT_FREE) {
			dbglib_slot_status[i] = DBGLIB_SLOT_BUSY;
			memset(&dbglib_samples[i], 0, N_SAMPLES_PER_SLOT * sizeof(s64));
			dbglib_count[i] = 0;
			return i;
		}
	}

	return 0xffffffff;
}

/**
 * Return 1 if the slot is full and ready to be used for the computation.
 */
int dbglib_collect_sample(uint32_t slot, s64 sample) {
	int ret = 0;

	dbglib_samples[slot][dbglib_count[slot]] = sample;

	if (unlikely(dbglib_count[slot] == N_SAMPLES_PER_SLOT - 1))
		ret = 1;

	dbglib_count[slot] = (dbglib_count[slot] + 1) % N_SAMPLES_PER_SLOT;

	return ret;
}

/**
 * Return 0 if no result is available yet.
 */
s64 dbglib_collect_sample_and_get_mean(uint32_t slot, s64 sample) {
	uint32_t i;
	s64 sum = 0;
	s64 result;

	if (unlikely(dbglib_collect_sample(slot, sample))) {
		for (i = 0; i < N_SAMPLES_PER_SLOT; i++)
			sum += dbglib_samples[slot][i];
		result = sum / N_SAMPLES_PER_SLOT;
		return result;
	}

	return 0;
}

void dbglib_collect_sample_and_show_mean(char *pre, uint32_t slot, s64 sample, char *post) {
	s64 result;

	if ((result = dbglib_collect_sample(slot, sample))) {
		lprintk(pre);
		lprintk("%u", (uint32_t) result);
		lprintk(post);
		lprintk("\n");
	}
}

void dbglib_init(void) {
	uint32_t i;

	for (i = 0; i < N_SLOTS; i++)
		dbglib_slot_status[i] = DBGLIB_SLOT_FREE;
}
