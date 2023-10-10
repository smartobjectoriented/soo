/*
 * Copyright (c) 2014, 2015, 2016, 2017 REDS Institute, HEIG-VD, Switzerland
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

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <lz4.h>
#include <lz4hc.h>

typedef unsigned char bool;

#define true 1
#define false 0

#define VERSION "1.0"

#define UPGRADABLE_COMPONENTS 3

typedef enum {
	ITB,
	UBOOT,
	ROOTFS
} component_type_t;

int lz4_compress_data(void **data_compressed, void *source_data, size_t source_size) {

	/* lz4_compressbound gives the worst reachable size after compression */
	void *compressed_data;
	size_t size_compressed;
	int ret;

	compressed_data = malloc(LZ4_compressBound(source_size));

	if ((size_compressed = LZ4_compress_HC((const char *)source_data, compressed_data, source_size,
					       LZ4_compressBound(source_size), 9)) < 0) {
		printf("Error when compressing the image\n");
		return ret;
	}

	*data_compressed = compressed_data;

	return size_compressed;
}

void *concat_img(unsigned char *img_ptr, char *img_name) {
	int fd;
	struct stat filestat;

	if (img_name != NULL) {
		stat(img_name, &filestat);

		fd = open(img_name, O_RDONLY);

		if (fd < 0) {
			perror(img_name);
			return NULL;
		}
		*((uint32_t *)img_ptr) = filestat.st_size;
		img_ptr += 4;

		read(fd, img_ptr, filestat.st_size);
		img_ptr += filestat.st_size;

		close(fd);
	}
	return img_ptr;
}

/*
 * Use as follows :
 * buildupdate -<type> <path_to_image> <version> [-<type> <path_to_image>
 * <version> ...] type can be: -k: itb -u: uboot -r: rootfs
 */
int main(int argc, char **argv) {
	char *components_filename[UPGRADABLE_COMPONENTS];
	bool components_to_upgrade[UPGRADABLE_COMPONENTS] = {false};
	uint32_t components_version[UPGRADABLE_COMPONENTS] = {0};

	unsigned char *img = NULL, *imgPtr;
	void *compressed_img;
	uint32_t compressed_size;

	int fdOut;
	struct stat filestat;

	bool upgrade_valid = false;

	/* 4 bytes for the image total size, UPGRADABLE_COMPONENTS * 4 bytes for the components
	* size Version number is not included as it is concatenated before the image
	* when writing to the final file.
	*/
	unsigned long img_size = UPGRADABLE_COMPONENTS * 4;
	int c, ret, i;
	bool ok = false;
	component_type_t type;

	printf("-- %s %s -- Generation of the update image...\n", argv[0], VERSION);

	if (argc < 4 || argc % 3 != 1) {
		printf("Usage: %s TYPE PATH VERSION [TYPE PATH VERSION ...]\n", argv[0]);
		printf("    -TYPE: k for itb, r for rootfs, u for uboot\n");
		return 1;
	}

	/* First pass over argv to get the image size needed for the
  	memory allocation */
	while ((c = getopt(argc, argv, "-:k:u:r:")) != -1) {
		switch (c) {
		case 'k':
			ok = true;
			type = ITB;
			break;
		case 'u':
			ok = true;
			type = UBOOT;
			break;
		case 'r':
			ok = true;
			type = ROOTFS;
			break;

		case ':':
			fprintf(stderr, "Option -%c requires an operand\n", optopt);
			break;
		case '?':
			fprintf(stderr, "Unrecognized option: -%c\n", optopt);
			break;
		}
		if (ok) {
			components_filename[(int)type] = argv[optind - 1];
			components_to_upgrade[(int)type] = true;
			components_version[(int)type] = strtoul(argv[optind], NULL, 10);
			ok = false;

			ret = stat(argv[optind - 1], &filestat);
			if (ret) {
				perror(argv[optind - 1]);
				exit(-1);
			}
			img_size += filestat.st_size;
		}
	}

	img = malloc(img_size);
	if (img == NULL)
		exit(-1);

	imgPtr = img;

	for (i = 0; i < UPGRADABLE_COMPONENTS; i++) {
		if (components_to_upgrade[i]) {
			printf("Concatenating image component type %d\n", i);
			imgPtr = concat_img(imgPtr, components_filename[i]);
			upgrade_valid = true;
		} else {
			*((uint32_t *)imgPtr) = 0;
			imgPtr += 4;
		}
	}

	if (!upgrade_valid) {
		printf("No valid component was found. Exiting without generating the "
		       "upgrade image.\n");
		return 0;
	}

	printf("Size of the image to compress: %ld bytes\n", img_size);

	compressed_size = lz4_compress_data(&compressed_img, img, img_size);

	fdOut = open("update.bin", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);

	write(fdOut, &img_size, 4);

	for (i = 0; i < UPGRADABLE_COMPONENTS; i++)
		write(fdOut, components_version + i, sizeof(components_version[i]));

	write(fdOut, compressed_img, compressed_size);

	free(compressed_img);

	close(fdOut);

	printf("Done!\n");

	return 0;
}
