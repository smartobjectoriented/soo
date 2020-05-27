#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <pthread.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <termios.h>

#include <injector/server.h>

#define MAXPATHLEN 30


static bool stopped = false;


void sig_term(int sig) {
	printf("End of the rfcomm session, restarting one...\n");
	stopped = true;
}

/*
 * Thread function which opens a RFCOMM socket and listen for 
 * incoming connection from the tablet or any BT device. 
 *
 * Code ported from BlueZ rfcomm.
 * https://github.com/pauloborges/bluez/blob/master/tools/rfcomm.c
 * 
 */
static void *BT_thread(void *dummy) {
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

	printf("My pid is %d\n", getpid());

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

void start_BT_server(void) {
	pthread_t t;

	pthread_create(&t, NULL, BT_thread, NULL);
}

