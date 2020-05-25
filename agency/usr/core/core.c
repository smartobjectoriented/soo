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
#include <unistd.h>

#include <sys/ioctl.h>

#include <core/core.h>
#include <core/upgrader.h>
#include <core/send.h>
#include <core/device_access.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/asf.h>

#include <dcm/core.h>

#include <injector/core.h>

#ifdef WITH_LED_ACTIVITIES
#include <leds/leds.h>
#endif


#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <pthread.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <errno.h>
#include <termios.h>
#include <fcntl.h>

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


#define MAXPATHLEN 30


static bool stopped = false;

void sig_term(int sig) {
	printf("End of the rfcomm session, restarting one...\n");
	stopped = true;
}


/*
 * Thread function which opens a RFCOMM socket and listen for 
 * incoming connection from the tablet. 
 * 
 */
static void *BT_thread(void *dummy) {

	return NULL;

	struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
	int s, client;
	socklen_t opt = sizeof(rem_addr);

	struct rfcomm_dev_req req;
	struct termios ti;

	int dev = 0;
	char devname[MAXPATHLEN];
	int fd;

	struct sigaction sa;

	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);

	while (1) {

		/* bind socket to channel 1 of the first available 
		 local bluetooth adapter */
		loc_addr.rc_family = AF_BLUETOOTH;
		loc_addr.rc_channel = (uint8_t) 1;

		s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

		if(bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
			perror("Can't bind RFCOMM socket");
			close(s);
			return NULL;
		}

		printf("Waiting for connection on channel %d\n", loc_addr.rc_channel);

		listen(s, 1);

		client = accept(s, (struct sockaddr *)&rem_addr, &opt);

		opt = sizeof(loc_addr);

		if (getsockname(client, (struct sockaddr *)&loc_addr, &opt) < 0) {
			perror("Can't get RFCOMM socket name");
			close(client);
			return NULL;
		}

		memset(&req, 0, sizeof(req));
		req.dev_id = dev;
		req.flags = (1 << RFCOMM_REUSE_DLC) | (1 << RFCOMM_RELEASE_ONHUP);

		bacpy(&req.src, &loc_addr.rc_bdaddr);
		bacpy(&req.dst, &rem_addr.rc_bdaddr);
		req.channel = rem_addr.rc_channel;

		dev = ioctl(client, RFCOMMCREATEDEV, &req);
		if (dev < 0) {
			perror("Can't create RFCOMM TTY");
			close(s);
			return NULL;
		}

		/* Opens the socket fd */
		snprintf(devname, MAXPATHLEN - 1, "/dev/rfcomm%d", dev);
		while ((fd = open(devname, O_RDONLY | O_NOCTTY)) < 0) {
			if (errno == EACCES) {
				perror("Can't open RFCOMM device");
				goto release;
			}
		}

		/* Make the RFCOMM socket raw to use TTY */
		tcflush(fd, TCIOFLUSH);
		cfmakeraw(&ti);
		tcsetattr(fd, TCSANOW, &ti);

		// close(s);
		// close(client);

		while (!stopped);
		stopped = false;
	}

   	return NULL;

release:
	memset(&req, 0, sizeof(req));
	req.dev_id = dev;
	req.flags = (1 << RFCOMM_HANGUP_NOW);
	ioctl(client, RFCOMMRELEASEDEV, &req);

	close(s);

   	return NULL;
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

	pthread_t th_bt;

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

	/* ASF initializations has to be performed before any thing else.  */
	asf_init();

	/* Display the agencyUID and visible neighbours at this time. */
	neigh_bitmap = -1;
	ioctl(fd_dcm, DCM_IOCTL_CONFIGURE_NEIGHBOURHOOD, &neigh_bitmap);

	signal(SIGINT, sig_agency_exit);
	signal(SIGKILL, sig_agency_exit);
	signal(SIGTERM, sig_agency_exit);
	signal(SIGUSR1, sig_agency_start);
	signal(SIGUSR2, sig_inject_ME_from_memory);

	pthread_create(&th_bt, NULL, BT_thread, NULL);

#ifdef WITH_LED_ACTIVITIES
	/* Initialize the LED Interface */
	leds_init();
#endif

	/* Initialzation of the DCM subsystem */
	dcm_init();

	migration_init();

	upgrader_init();

	injector_init();

#ifdef WITH_LED_ACTIVITIES
	for (i = 0 ; i < SOO_N_LEDS ; i++)
		led_off(i + 1);

	led_on(1);
#endif

	/* Automatically inject the MEs in the /ME directory */
	inject_MEs_from_filesystem();

	/* Start the main loop of the Agency Core subsystem */
	main_loop(AG_CYCLE_PERIOD);

	return 0;
}
