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

#ifndef VWEATHER_H
#define VWEATHER_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <soo/uapi/soo.h>

#define VWEATHER_NAME		"vweather"
#define VWEATHER_PREFIX		"[" VWEATHER_NAME "] "


/* ASCII data coming from the weather station */
typedef struct {
	char	frame_begin[1];	/* 'W' */
	char	temperature[5];	/* Outdoor temperature in Celsius degrees: '+'/'-'ab.c */
	char	south_sun[2];	/* South Sun in klx: ab */
	char	west_sun[2];	/* West Sun in klx: ab */
	char	east_sun[2];	/* East Sun in klx: ab */
	char	twilight[1];	/* Twilight: 'J'/'N' */
	char	light[3];	/* Light intensity in lx: abc */
	char	wind[4];	/* Wind speed in m/s: ab.c */
	char	rain[1];	/* Rain: 'J'/'N' */
	char	week_day[1];	/* Day of the week: '1'..'7', from Monday to Sunday */
	char	day[2];		/* Day */
	char	month[2];	/* Month */
	char	year[2];	/* Year on two digits */
	char	hour[2];	/* Hour */
	char	minute[2];	/* Minute */
	char	second[2];	/* Second */
	char	summer_time[1];	/* Summer time: 'J'/'N'/'?' */
	char	checksum[4];	/* Checksum: abcd */
	char	frame_end[1];	/* 3 */
} vweather_ascii_data_t;

/* Rain intensity */
typedef enum {
	NO_RAIN,	/* 0mm/hr */
	LIGHT_RAIN,	/* <3mm/hr */
	MODERATE_RAIN,	/* >=3 <8mm/hr */
	HEAVY_RAIN	/* >=8mm/h */
} rain_intensity_t ;

/* Weather data spread over the ecosystem */
typedef struct {
	uint32_t		south_sun;	/* South Sun in klx */
	uint32_t		west_sun;	/* West Sun in klx */
	uint32_t		east_sun;	/* East Sun in klx */
	uint32_t		light;		/* Light in lx */
	int32_t			temperature;	/* Temperature in Celsius degrees */
	uint32_t		wind;		/* Wind speed in m/s */
	rain_intensity_t	rain_intensity;	/* Rain intensity */
	uint8_t			twilight;	/* Twilight: 1 or 0 */
	uint8_t			rain;		/* Rain: 1 or 0 */

	struct timer_list rain_timer;
} vweather_data_t;

/* 40 bytes */
#define VWEATHER_FRAME_SIZE		sizeof(vweather_ascii_data_t)

/* Data accessor */
#define VWEATHER_GET_ASCII_DATA(output, data_set, name) \
	memcpy(output, &data_set.name[0], sizeof(data_set.name)); \
	output[sizeof(data_set.name)] = '\0';

#define VWEATHER_DATA_SIZE	sizeof(vweather_data_t)

/* PD4: GPIO number = (4 - 1) x 32 + 4 = 100 */
#define UART4_RTS_GPIO	100

/* PL2: GPIO number = (12 - 1) x 32 + 2 = 354 */
#define PLUVIO_GPIO	354

/*
 * Amount of rain that triggers a pluviometer interrupt. This value must be divided by 10000 to get
 * the real one in mm.
 */
#define PLUVIO_THRESHOLD	2794

/* Rain amount thresholds, expressed in delay between two pluviometer interrupts, in minutes */

/* Moderate rain: 10 interrupts/hr => 1 interrupt/6 minutes */
/* (expressed in ms) */
#define MODERATE_RAIN_INT_DELAY	(6 * 60 * 1000)

/* Heavy rain: 28 interrupts/hr => 1 interrupt/2 minutes */
#define HEAVY_RAIN_INT_DELAY	(2 * 60 * 1000)

/* Very light rain: 1 interrupt/hour */
#define LIGHT_RAIN_INT_DELAY	(60 * 60 * 1000)

/* Rain state reset delay */
//#define RAIN_RESET_DELAY	MILLISECS_IN_30MINUTES

void vweather_dump_ascii_data(void);
void vweather_dump_data(void);

typedef struct {
	char		*data;
	unsigned int	pfn;
} vweather_shared_buffer_t;

typedef struct {
	unsigned int	irq;
} vweather_notification_t;

typedef struct {
    vdevback_t vdevback;
	vweather_shared_buffer_t	weather_buffers;
	vweather_notification_t		update_notifications;

} vweather_t;

extern vweather_t vweather;

extern size_t vweather_packet_size;

/* State management */
void vweather_probe(struct vbus_device *dev);
void vweather_close(struct vbus_device *dev);
void vweather_suspend(struct vbus_device *dev);
void vweather_resume(struct vbus_device *dev);
void vweather_connected(struct vbus_device *dev);
void vweather_reconfigured(struct vbus_device *dev);
void vweather_remove(struct vbus_device *dev);

/* Shared buffer setup */
int vweather_setup_shared_buffer(struct vbus_device *dev);

void vweather_vbus_init(void);

bool vweather_start(domid_t domid);
void vweather_end(domid_t domid);
bool vweather_is_connected(domid_t domid);

irqreturn_t vweather_interrupt(int irq, void *dev_id);

#endif /* VWEATHER_H */
