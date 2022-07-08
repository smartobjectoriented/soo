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
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <frozen.h>
#include <lz4.h>

#include <core/core.h>
#include <core/debug.h>
#include <core/device_access.h>
#include <core/receive.h>
#include <core/send.h>
#include <core/types.h>
#include <core/upgrader.h>

#include <soo/uapi/soo.h>

/* Used by the polling function to know when to stop */
bool upgrade_done = false;

/**
 * JSON helper functions
 */
static int replace_int_json(char *filename, const char *attribute, int value) {
	char *tmp = json_fread(AGENCY_JSON_PATH);
	FILE *fp = fopen(filename, "w");
	struct json_out out = JSON_OUT_FILE(fp);

	json_setf(tmp, strlen(tmp), &out, attribute, "%d", value);

	free(tmp);
	fclose(fp);

	return 0;
}

static void read_version_number_json(upgrade_versions_args_t *versions_args) {
	char *agency_metadatas;

	agency_metadatas = json_fread(AGENCY_JSON_PATH);

	if (agency_metadatas == NULL) {
		fprintf(stderr, "Error reading the agency.json data.\n");
		BUG();
	}

	json_scanf(agency_metadatas, strlen(agency_metadatas), "{.itb: %d}", &versions_args->itb);
	json_scanf(agency_metadatas, strlen(agency_metadatas), "{.uboot: %d}", &versions_args->uboot);
	json_scanf(agency_metadatas, strlen(agency_metadatas), "{.rootfs: %d}", &versions_args->rootfs);

	free(agency_metadatas);
}

/**
 * Upgrade the agency components. It also is in charge to modify the agency.json 
 * metadata.
 * Params:
 * 	args: Contains information about the upgrade. Used to pass the upgrade header
 * 	      and the decompressed upgrade address.
 */
static void upgrade_components(upgrader_args_t *args) {

	char *img_cpy = args->decompressed_image;
	uint32_t comp_size;
	int i;

	for (i = 0; i < UPGRADABLE_COMPONENTS; ++i) {
		comp_size = *((uint32_t *)img_cpy);

		if (comp_size != 0) {
			printf("Upgrader: upgrade component type: %d uncompressed size: %u bytes.\n", i, comp_size);
			switch (i) {
			case ITB:
				printf("Upgrader: upgrading agency itb file...\n");
				upgrade_itb(img_cpy + 4, comp_size);
				replace_int_json(AGENCY_JSON_PATH, ".itb", *((char *)args->header + 4));
				break;
			case UBOOT:
				printf("Upgrader: upgrading U-boot...\n");
				/* TODO: upgrade_uEnv_txt(upgrade_image+COMPONENT_HEADER_SIZE, size); */
				replace_int_json(AGENCY_JSON_PATH, ".uboot", *((char *)args->header + 8));
				break;
			case ROOTFS:
				printf("Upgrader: upgrading the rootfs...\n");
				upgrade_rootfs(img_cpy + 4, comp_size);
				replace_int_json(AGENCY_JSON_PATH, ".rootfs", *((char *)args->header + 12));

				break;
			default:
				printf("No such component (%d)...\n", i);
				BUG();
			}
		}
		img_cpy += 4 + comp_size;
	}
}

/**
 * Check if the upgrade was stored by the SOO.agency ME.
 * It retrieve the upgrade size using ioctl.
 * 
 * Params:
 * 	args [OUT]: Contains the information about the upgarde. After this function,
 * 		       it is updated with the upgrade image size and the ME slot from 
 * 		       which the upgarde was stored.
 * 
 * Return: true if the upgarde is present, false otherwise.
 */
static bool is_upgrade_present(upgrader_args_t *args) {
	agency_ioctl_args_t agency_ioctl_args;

	args->compressed_size = 0;

	/* Try to retrieve the upgrade image address and size */
	if ((ioctl(fd_core, AGENCY_IOCTL_GET_UPGRADE_IMG, &agency_ioctl_args)) < 0) {
		DBG0("ioctl UPGRADER_IOCTL_IMG failed.\n");
		BUG();
	}
	/* If the size returned in value is 0, it means that no upgrade is available */
	if (agency_ioctl_args.value == 0)
		return false;

	args->compressed_size = agency_ioctl_args.value;
	args->ME_slotID = agency_ioctl_args.slotID;

	return true;
 }

/**
 * Map the upgrade image from the kernel space and decompress it.
 * 
 * Params:
 * 	args [IN/OUT]: Contains the information about the upgarde. After this function,
 * 		       it is updated with the header and decompressed image addresses,
 * 		       be careful that you MUST free them after.
 */
static void retrieve_image(upgrader_args_t *args) {
	void *shmem;
	void *decompressed_img;
	uint32_t decompressed_size;

	if (args->compressed_size == 0) 
		return;
	
	/* Map the upgarde image */
	shmem = mmap(NULL, args->compressed_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_core, 0);
	if (shmem == NULL) {
		printf("An error occured during mmap of the upgrade\n");
		return;
	}

	/* The first 4 bytes of the upgrade image are the decompressed size */
	decompressed_size = *((uint32_t *)shmem);
	printf("Upgrader: decompressed size: %u\n", decompressed_size);

	decompressed_img = malloc(decompressed_size);
	if (decompressed_img == NULL) {
		printf("Could not malloc the decompressed image\n");
		BUG();
	}

	printf("Decompressing the image...");
	/* Decompress the payload using LZ4 */
	LZ4_decompress_safe((char *)(shmem + HEADER_SIZE), (char *)decompressed_img, args->compressed_size - HEADER_SIZE, decompressed_size);
	printf(" done!\n");

	args->decompressed_image = (char *)decompressed_img;
	args->header = (char *)shmem;
}

/**
 * Unmap the previously mapped upgrade image and free the decompressed image buffer.
 * It then notifies the ME using the localinfo_update callback. 
 * Finally it asks for a system reboot.
 * 
 * Params:
 * 	args [IN]: Contains the information about the upgarde.
 */
static void finish_upgrade(upgrader_args_t *args) {
	upgrade_versions_args_t args_upgrade;
	agency_ioctl_args_t agency_ioctl_args;
	ME_id_t me_id;

	free(args->decompressed_image);
	munmap(args->header, args->compressed_size);

	/* Update the version numbers in VBStore */
	read_version_number_json(&args_upgrade);
	if ((ioctl(fd_core, AGENCY_IOCTL_STORE_VERSIONS, &args_upgrade)) < 0) {
		DBG0("ioctl AGENCY_IOCTL_STORE_VERSIONS failed.\n");
		BUG();
	}

	printf("Updated version numbers: ITB %u, rootfs %u, uboot %u\n", args_upgrade.itb, args_upgrade.rootfs, args_upgrade.uboot);

	/* notify ME that we finished the upgrade */
	printf("Upgrader: Upgrade done.\n");

	/* Wait for the ME to be in the living state, so we ensure that the localinfo
	   callback is called in the ME. */
	agency_ioctl_args.slotID = args->ME_slotID;
	agency_ioctl_args.buffer = &me_id;
	do {
		if ((ioctl(fd_core, AGENCY_IOCTL_GET_ME_ID, &agency_ioctl_args)) < 0) {
			DBG0("ioctl AGENCY_IOCTL_GET_ME_ID failed.\n");
			BUG();
		}
	} while (me_id.state != ME_state_living);
	
	upgrade_done = true;

	/* We initiate a reboot of the system including shutdowning the user space properly. */
	system("reboot");
}

/**
 * Polling function used by the upgrader thread.
 * It polls every <UPGRADE_POLL_PERIOD_MS> to check if a SOO.agency is present.
 * If so, it means the agency should be upgraded, the version numbers checks are
 * done by the ME.
 * 
 */
#warning To replace polling mechanism with a blocking sync...
void *upgrade_poll_fct(void *param) {
	/* We use these ioctl args to pass information between the different steps */
	upgrader_args_t args;

	memset(&args, 0, sizeof(upgrader_args_t));

	while (!upgrade_done) {
		usleep(UPGRADE_POLL_PERIOD_MS * 1000);
		if (is_upgrade_present(&args)) {
			retrieve_image(&args);
			upgrade_components(&args);
			finish_upgrade(&args);
		}
	}

	return NULL;
}

/*
 * Initializing the upgrader functional block.
 * The general fd_core descriptor will be used to handle IOCTL requests.
 */
void upgrader_init(void) {
	upgrade_versions_args_t args;
	agency_ioctl_args_t agency_ioctl_args;

	/* Reads the agency version numbers from the JSON and store them in VBStore */
	read_version_number_json(&args);

	agency_ioctl_args.buffer = &args;

	if ((ioctl(fd_core, AGENCY_IOCTL_STORE_VERSIONS, &agency_ioctl_args)) < 0) {
		DBG0("ioctl AGENCY_IOCTL_STORE_VERSIONS failed.\n");
		BUG();
	}

#warning Upgrader.. To be reworked out with a non-polling mechanism
#if 0
	pthread_create(&th1, NULL, upgrade_poll_fct, NULL);
#endif
}
