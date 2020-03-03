/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#if 0
#define DEBUG
#endif

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <kconfig.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <core/core.h>
#include <core/upgrader.h>
#include <core/send.h>
#include <core/device_access.h>
#include <core/debug.h>
#include <core/types.h>

#include <dcm/core.h>

#if defined(CONFIG_LEDS)
#include <leds/leds.h>
#endif /* CONFIG_LEDS */

#define AGENCY_CORE_VERSION "2019.2"

static volatile bool __started = false;
/*
 * The following options are available for the agency application
 */
bool opt_noinject = false;
bool opt_nosend = false;

bool ag_cycle_interrupted = false;

/*
 * Executed on the arrival of the SIGINT/SIGTERM/SIGKILL signal.
 */
static void sig_agency_exit(int sig) {
	ag_cycle_interrupted = true;
}

static void sig_agency_start(int sig) {
	printf("Agency starting now...\n");

	__started = true;
}

/**
 * Main entry point of the Agency core subsystem.
 */
int main(int argc, char *argv[]) {
	uint32_t i;
	uint32_t neigh_bitmap = -1;
	char *token;
	int rc;
	uint8_t val;

	printf("SOO Agency core application.\n");
	printf("Version: %s\n", AGENCY_CORE_VERSION);
	printf("*****************************************************\n\n\n");
	printf("	-noinject : to prevent the load of Mobile Entities\n");
	printf("        -uid      : to assign a specific agencyUID (only the last byte, the others are at 00)\n");
	printf("	-nosend   : to prevent the broadcast of MEs\n");
	printf("        -d	  : to list the known neighbours (this will also enable the Discovery process).\n");
	printf("        -w        : wait until receiving SIGUSR1 (can thus be started from udev)\n");
	printf("	-n x,y,z  : to consider the neighbour which appears at the x-th, y-th, and z-th position in the neighbour list.\n");
	printf("        -r        : reset the bitmap defined with '-n' option\n");
	printf("\n\n");

	dcm_dev_init();

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-uid")){
			val = atoi(argv[i+1]);
			if ((rc = ioctl(fd_dcm, DCM_IOCTL_SET_AGENCY_UID, val)) < 0) {
				printf("Failed to access the neighbourhood (%d)\n", rc);
				BUG();
			}

			close(fd_dcm);

			exit(0);
		}

		if (!strcmp(argv[i], "-noinject"))
			opt_noinject = true;

		if (!strcmp(argv[i], "-nosend"))
			opt_nosend = true;

		/* -n option is to activate only a number of neighbours.
		 * For instance -n 1,3,4 means that the first neighbour,
		 * third and fourth neighbours of the discovery list
		 * will be considered, other will be ignored.
		 * Neighbour starts at 1.
		 */

		if (!strcmp(argv[i], "-n")) {
			neigh_bitmap = 0;
			token = strtok(argv[i+1], ",");
			while (token != NULL) {
				neigh_bitmap |= 1 << (atoi(token)-1);
				token = strtok(NULL, ",");
			}

			if ((rc = ioctl(fd_dcm, DCM_IOCTL_CONFIGURE_NEIGHBOURHOOD, &neigh_bitmap)) < 0) {
				printf("Failed to access the neighbourhood (%d)\n", rc);
				BUG();
			}

			close(fd_dcm);
			exit(0);
		}

		if (!strcmp(argv[i], "-w")) {
			printf("Agency: waiting to start via SIGUSR1...\n");
			signal(SIGUSR1, sig_agency_start);
			while (!__started) ;
		}

		if (!strcmp(argv[i], "-d")) {
			neigh_bitmap = -1;
			if ((rc = ioctl(fd_dcm, DCM_IOCTL_CONFIGURE_NEIGHBOURHOOD, &neigh_bitmap)) < 0) {
				printf("Failed to access the neighbourhood (%d)\n", rc);
				BUG();
			}

			close(fd_dcm);
			exit(0);
		}

		if (!strcmp(argv[i], "-r")) {
			if ((rc = ioctl(fd_dcm, DCM_IOCTL_RESET_NEIGHBOURHOOD, 0)) < 0) {
				printf("Failed to reset the neighbourhood (%d)\n", rc);
				BUG();
			}

			close(fd_dcm);
			exit(0);
		}
	}

	/* Display the agencyUID and visible neighbours at this time. */
	neigh_bitmap = -1;
	ioctl(fd_dcm, DCM_IOCTL_CONFIGURE_NEIGHBOURHOOD, &neigh_bitmap);

	signal(SIGINT, sig_agency_exit);
	signal(SIGKILL, sig_agency_exit);
	signal(SIGTERM, sig_agency_exit);
	signal(SIGUSR1, sig_agency_start);
	signal(SIGUSR2, sig_inject_ME_from_memory);

#if defined(CONFIG_LEDS)
	/* Initialize the LED Interface */
	leds_init();
#endif /* CONFIG_LEDS */

	/* Initialzation of the DCM subsystem */
	dcm_init();

	migration_init();

	upgrader_init();

#if defined(CONFIG_LEDS)
	for (i = 0 ; i < SOO_N_LEDS ; i++)
		led_off(i + 1);

	led_on(1);
#endif /* CONFIG_LEDS */

	/* Automatically inject the MEs in the /ME directory */
	inject_MEs_from_filesystem();

	/* Start the main loop of the Agency Core subsystem */
	main_loop(AG_CYCLE_PERIOD);

	return 0;
}
