 /* 
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (c) 2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef DEVICE_ACCESS_H
#define DEVICE_ACCESS_H

#define FILENAME_MAX_LEN	80

#define SOO_ME_DIRECTORY 	"/mnt/ME"
#define SOO_BOOT_PART		"/mnt/boot/"
#define SOO_ROOTFS_P2 		"/mnt/rootfs1/"
#define SOO_ROOTFS_P4 		"/mnt/rootfs2/"
#define ROOT_PATH 		"/root/"

#define ROOTFS1_PART		'2'
#define ROOTFS2_PART		'4'

#define ROOTFS_ARCHIVE_NAME 	"rootfs.tar"
#define AGENCY_JSON_NAME 	"agency.json"
#define AGENCY_JSON_PATH 	SOO_BOOT_PART AGENCY_JSON_NAME
#define ROOT_FLAG_NAME		"root_flag"
#define ROOT_FLAG_PATH 		SOO_BOOT_PART ROOT_FLAG_NAME
#define UENV_TXT_NAME		"uEnv.txt"
#define UENV_TXT_PATH 		SOO_BOOT_PART UENV_TXT_NAME

#define ITB_NAME		"soo.itb"

#define ITB_PATH 		SOO_BOOT_PART ITB_NAME

void agency_inject_ME_from_memory(void);
void sig_inject_ME_from_memory(int sig);

void inject_MEs_from_filesystem(void);

int upgrade_uEnv_txt(char *uEnv_updated, size_t size);
int upgrade_itb(char *itb_updated, size_t size);
int upgrade_rootfs(char *rootfs_updated, size_t size);

void device_access_init(void);

#endif /* DEVICE_ACCESS_H */
