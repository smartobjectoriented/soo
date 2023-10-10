/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

/*
 * Device access functional block of the agency core
 */

#if 0
#define DEBUG
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <core/core.h>
#include <core/debug.h>
#include <core/device_access.h>
#include <core/injector.h>

#include <archive.h>
#include <archive_entry.h>


/* Both folowing functions come from libarchive examples wiki: 
   https://github.com/libarchive/libarchive/wiki/Examples */
static int copy_data(struct archive *ar, struct archive *aw) {
	int r;
	const void *buff;
	size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
	int64_t offset;
#else
	off_t offset;
#endif

	for (;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r != ARCHIVE_OK)
			return (r);
		r = archive_write_data_block(aw, buff, size, offset);
		if (r != ARCHIVE_OK) {
			printf("archive_write_data_block()\n");
			return (r);
		}
	}
}

/**
 * Extract any type of archive using the libarchive library. It
 * extracts it in the current directory.
 * Params:
 * 	filename: Path to the file to extract. 
 */ 
static void extract(const char *filename) {
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int flags;
	int r;

	/* Select which attributes we want to restore. */
	flags = ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_ACL;
	flags |= ARCHIVE_EXTRACT_FFLAGS;

	a = archive_read_new();
	archive_read_support_format_all(a);
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);
	archive_write_disk_set_standard_lookup(ext);
	if ((r = archive_read_open_filename(a, filename, 10240))) {
		printf("archive_read_open_filename failed!\n");
		return;
	}
	for (;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(a));
		if (r < ARCHIVE_WARN)
			printf("ARCHIVE_WARN\n");
		r = archive_write_header(ext, entry);
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(ext));
		else if (archive_entry_size(entry) > 0) {
			r = copy_data(a, ext);
			if (r < ARCHIVE_OK)
				fprintf(stderr, "%s\n", archive_error_string(ext));
			if (r < ARCHIVE_WARN)
				printf("ARCHIVE_WARN\n");
		}
		r = archive_write_finish_entry(ext);
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(ext));
		if (r < ARCHIVE_WARN)
			printf("ARCHIVE_WARN\n");
	}
	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);
}

/**
 * Upgrades the uEnv.txt file located in the boot partition.
 *
 * Params:
 * 	uEnv_upgarded: Pointer to the begining of the new uEnv.txt data.
 * 	size: Size of the new uEnv.txt data. 
 * 
 * Return: 0 if success, -1 otherwise
 */ 
int upgrade_uEnv_txt(char *uEnv_updated, size_t size) {
	FILE *fp;
	int bw, ret = 0;

	fp = fopen(UENV_TXT_PATH, "w");
	if (fp == NULL) {
		printf("Could not open %s!\n", UENV_TXT_PATH);
		return -1;
	}

	bw = fwrite(uEnv_updated, sizeof(char), size, fp);
	
	if (bw != size) {
		printf("%s: error occured during the fwrite of %s. ret = %d\n", __func__, UENV_TXT_NAME, ret);
		ret = -1;
	}

	fclose(fp);

	return ret;
}

/**
 * Upgrades the ITB located in the boot partition.
 *
 * Params:
 * 	itb_updated: Pointer to the beginning of the new ITB binary data.
 * 		     Needs to be already uncompressed.
 * 	size: Size of the new ITB data. 
 * 
 * Return: 0 if success, -1 otherwise
 */ 
int upgrade_itb(char *itb_updated, size_t size) {
	FILE *fp;
	int bw, ret = 0;

	fp = fopen(ITB_PATH, "wb");
	if (fp == NULL) {
		printf("Could not open %s!\n", ITB_PATH);
		return -1;
	}

	bw = fwrite(itb_updated, sizeof(char), size, fp);
	if (bw != size) {
		printf("%s: error occured during the fwrite of %s. ret = %d\n", __func__,  ITB_NAME, ret);
		ret = -1;
	}
	fclose(fp);

	return ret;
}

/**
 * Upgrade the rootfs. It needs to be an uncompressed archive.
 * If the upgrade finishes without error, the root partition is updated using 
 * the root_flag located in the boot partition. It allows to switch between 
 * partiton 2 and 4 as root partition. The partition used as rootfs before the
 * upgrade becomes a backup partition.
 * 
 * Params:
 * 	rootfs_upgraded: Pointer to the begining of the new archived rootfs.
 * 	size: Size of the new rootfs archive. 
 *
 * Return: 0 if success, -1 otherwise
 */
int upgrade_rootfs(char *rootfs_upgraded, size_t size) {
	FILE *fp_root_flag;
	FILE *fp_rootfs = NULL;
	int ret;
	char new_path[100];
	char cur_root_part;
	char *tar_path;

	printf("Upgrader: rootfs upgrade\n");

	memset(new_path, '\0', sizeof(new_path));

	/* Open and read the root_flag that indicates the current rootfs partition */
	fp_root_flag = fopen(ROOT_FLAG_PATH, "r+");
	if (fp_root_flag == NULL) {
		printf("Could not open root_flag!\n");
		return -1;
	}
	ret = fread(&cur_root_part, sizeof(char), 1, fp_root_flag);
	if (ret != 1) {
		printf("Error during reading root_flag!\n");
		fclose(fp_root_flag);
		return -1;
	}
	/* Check the current rootfs and change it accordingly */
	if (cur_root_part == ROOTFS1_PART) {
		cur_root_part = ROOTFS2_PART;
		tar_path = SOO_ROOTFS_P4;
	} else if (cur_root_part == ROOTFS2_PART) {
		cur_root_part = ROOTFS1_PART;
		tar_path = SOO_ROOTFS_P2;
	} else {
		printf("The root_flag contains an incorrect partition number!\n");
		fclose(fp_root_flag);
		return -1;
	}

	/* Create the new rootfs path string */
	strcpy(new_path, tar_path);
	strcat(new_path, ROOTFS_ARCHIVE_NAME);

	printf("Creating file %s\n", new_path);

	fp_rootfs = fopen(new_path, "wb");
	if (fp_rootfs == NULL) {
		printf("Error creating the %s file!\n", ROOTFS_ARCHIVE_NAME);
		fclose(fp_root_flag);
		fclose(fp_rootfs);
		return -1;
	}
	/* Write the archive */
	ret = fwrite(rootfs_upgraded, sizeof(char), size, fp_rootfs);
	if (ret != size) {
		printf("An error occured during the fwrite of rootfs.tar. ret = %d\n", ret);
		fclose(fp_root_flag);
		fclose(fp_rootfs);
		return -1;
	}

	/* Write the new root dev partition number into the root_flag */
	rewind(fp_root_flag);
	fwrite(&cur_root_part, sizeof(char), 1, fp_root_flag);
	fclose(fp_root_flag);

	fclose(fp_rootfs);

	/* Place ourself in the new rootfs mounted partition */
	chdir(tar_path);
	printf("Upgarder: Extracting the rootfs...\n");


	extract(new_path);
	printf("Done!\n");

	/* Remove the archive */
	printf("Upgarder: Removing the archive...\n");
	remove(new_path);
	printf("Done!\n");

	return 0;
}

void device_access_init(void) {

	/* Nothing */
}
