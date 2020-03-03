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

#ifndef VAUDIO_H
#define VAUDIO_H

#include <soo/ring.h>

#if defined(CONFIG_SOO_ME)
#include <ioctl.h>
#endif /* CONFIG_SOO_ME */

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif /* !__KERNEL__ */

#ifdef __KERNEL__
#define VAUDIO_NAME	"vaudio"
#define VAUDIO_PREFIX	"[" VAUDIO_NAME "] "
#endif /* __KERNEL__ */

/* REC = record, non-RT mode */

#define VAUDIO_REC_SAMPLING_FREQUENCY	32000
#define VAUDIO_REC_WIDTH			16
#define VAUDIO_REC_PHYSICAL_WIDTH		32
#define VAUDIO_REC_N_CHANNELS		1

#define VAUDIO_REC_DURATION		3
#define VAUDIO_REC_N_SAMPLES		(VAUDIO_REC_N_CHANNELS * VAUDIO_REC_DURATION * VAUDIO_REC_SAMPLING_FREQUENCY)
#define VAUDIO_REC_BUFFER_SIZE		((VAUDIO_REC_WIDTH / 8) * VAUDIO_REC_N_SAMPLES)

#define VAUDIO_EMPTY			0
#define VAUDIO_IN_PROGRESS		0xf
#define VAUDIO_FINISHED			0xf0

/* STR = stream, RT mode */

#define VAUDIO_STR_SAMPLING_FREQUENCY	48000
#define VAUDIO_STR_WIDTH			24
#define VAUDIO_STR_PHYSICAL_WIDTH		32
#define VAUDIO_STR_N_CHANNELS		2

#define VAUDIO_STR_N_SAMPLES_PER_PERIOD		48
#define VAUDIO_STR_MAX_SAMPLES_PER_PERIOD	64
#define VAUDIO_STR_BUFFER_SIZE			((VAUDIO_STR_PHYSICAL_WIDTH / 8) * VAUDIO_STR_MAX_SAMPLES_PER_PERIOD)

typedef struct {
	uint8_t	recording_status;
	uint8_t	playback_status;
	uint8_t	audio_data[0];
} vaudio_data_t;

/*
 * We are using double audio buffers with:
 * - The recorded data in the first half.
 * - The data to be played in the second half.
 */
#define VAUDIO_REC_DATA_SIZE	(sizeof(vaudio_data_t) + 2 * VAUDIO_REC_BUFFER_SIZE)
#define VAUDIO_STR_DATA_SIZE	(sizeof(vaudio_data_t) + 2 * VAUDIO_STR_BUFFER_SIZE)

#define VAUDIO_IOCTL_REQ_RECORDING	_IOW(0x500a6d10u, 1, uint8_t)
#define VAUDIO_IOCTL_REQ_PLAYBACK	_IOW(0x500a6d10u, 2, uint8_t)
#define VAUDIO_IOCTL_ENABLE_RT		_IOW(0x500a6d10u, 3, uint8_t)
#define VAUDIO_IOCTL_START_RT_STREAM	_IOW(0x500a6d10u, 4, uint8_t)
#define VAUDIO_IOCTL_STOP_RT_STREAM	_IOW(0x500a6d10u, 5, uint8_t)

#define VAUDIO_MAX_COMMANDS	8

typedef struct {
	uint32_t cmd;
} vaudio_cmd_request_t;

/* Not used */
typedef struct {
	uint32_t val;
} vaudio_cmd_response_t;

DEFINE_RING_TYPES(vaudio_cmd, vaudio_cmd_request_t, vaudio_cmd_response_t);

#if defined(CONFIG_SOO_AGENCY)

/* Non-RT */
void vaudio_set_recording_status(uint8_t status);
void vaudio_set_playback_status(uint8_t status);

void rtdm_audio_reconfigure(void);

/* RT */

void vaudio_play(uint32_t *samples, uint32_t n_samples, uint32_t n_sample_offset);
void vaudio_capture(uint32_t *samples, uint32_t n_samples, uint32_t n_sample_offset);
void vaudio_hw_interrupt(void);

#endif /* CONFIG_SOO_AGENCY */

#if defined(CONFIG_SOO_ME)

/* Non-RT */

void vaudio_request_recording(void);
uint8_t vaudio_get_recording_status(void);
void vaudio_get_recorded_data(void *dest);
void vaudio_release_recorded_data(void);

void vaudio_set_play_data(void *src);
void vaudio_request_playback(void);
uint8_t vaudio_get_playback_status(void);
void vaudio_release_played_data(void);

/* RT */

void vaudio_enable_rt(bool realtime);
void vaudio_start_stream(void);
void vaudio_stop_stream(void);
void vaudio_capture(uint32_t **samples, uint32_t *n_samples);
void vaudio_play(uint32_t *samples, uint32_t n_samples);

#endif /* CONFIG_SOO_ME */

#endif /* VAUDIO_H */
