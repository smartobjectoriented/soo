/*
 * Copyright (C) 2019 David Truan <david.truan@heig-vd.ch>
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

#ifndef UPGRADER_H
#define UPGRADER_H

#define UPGRADABLE_COMPONENTS 3
#define UPGRADE_POLL_PERIOD_MS 500

/** 
 * Image format: | uncompressed size | ITB version | U-boot version | rootfs version | <payload> 
 * The payload is composed of the 3 components as follows: <img size | img> for each component. 
 * Each field except the images themself are on 4B unsigned 
 * 
 */ 
#define HEADER_SIZE (4 + 3 * 4)

typedef enum {
	ITB,
	UBOOT,
	ROOTFS
} component_type_t;

/* Used to pass information between the upgarde stages */ 
typedef struct {
	char *header;
	size_t	compressed_size;
	char *decompressed_image;
    	unsigned int ME_slotID;
} upgrader_args_t;

void upgrader_init(void);

#endif /* UPGRADER_H */
