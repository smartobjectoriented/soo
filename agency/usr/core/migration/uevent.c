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

#define VERBOSE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#include <soo/uapi/soo.h>

#include <core/core.h>

#define SOO_UEVENT_PATH "/sys/devices/system/soo/soo0/uevent"
#define SOO_UEVENT_CONF "/etc/soo/eventd.conf"

static char scriptFile[ME_EVENT_NR][80];

int process_uevent(int fd) {
	int fd_uevent;
	char buf[80], *uevent_cmd;
	char *pos;
	const char *delim = "=:";
	unsigned ME_slotID, rc;

	fd_uevent = open(SOO_UEVENT_PATH, O_RDONLY);
	read(fd_uevent, buf, 80);
	pos = strstr(buf, "SOO_CALLBACK=");

	uevent_cmd = strsep(&pos, delim); /* SOO_CALLBACK */
	uevent_cmd = strsep(&pos, delim);

	/* FORCE_TERMINATE processing */
	if (!strcmp(uevent_cmd, "FORCE_TERMINATE")) {
		uevent_cmd = strsep(&pos, delim); /* Get the slot ID */

		sscanf(uevent_cmd, "%u", &ME_slotID);

		rc = ioctl(fd, AGENCY_IOCTL_FORCE_TERMINATE, &ME_slotID);
		if (rc < 0) {
			printf("%s: FORCE_TERMINATE ioctl failed with %d\n", __func__, rc);
			return rc;
		}

	}

	if (!strcmp(uevent_cmd, "LOCALINFO_UPDATE")) {
		uevent_cmd = strsep(&pos, delim); /* Get the slot ID */

		sscanf(uevent_cmd, "%u", &ME_slotID);

		rc = ioctl(fd, AGENCY_IOCTL_LOCALINFO_UPDATE, &ME_slotID);
		if (rc < 0) {
			printf("%s: LOCALINFO_UPDATE ioctl failed with %d\n", __func__, rc);
			return rc;
		}

	}
	close(fd_uevent);

	return 0;
}

/*
 * uevent thread to process events dedicated to the agency core
 */
void check_for_uevent(void) {

	int found, rc = 0, fd;
	char buf[80];
	FILE *conf_file;
#ifdef DEBUG
	int i;
#endif

	if ((conf_file = fopen(SOO_UEVENT_CONF, "r")) == NULL) {
		printf("Cannot open " SOO_UEVENT_CONF "\n");
		return ;
	}

	while (!feof(conf_file)) {
		fgets(buf, 80, conf_file);

		/* Skip # comment */
		if (buf[0] == '#')
			continue;

		if (strstr(buf, "FORCE_TERMINATE"))
			sscanf(buf, "FORCE_TERMINATE=%s", scriptFile[ME_FORCE_TERMINATE]);

		if (strstr(buf, "LOCALINFO_UPDATE"))
		  sscanf(buf, "LOCALINFO_UPDATE=%s", scriptFile[ME_LOCALINFO_UPDATE]);

	}


	fclose(conf_file);

#ifdef DEBUG
	for (i = 0; i < ME_EVENT_NR; i++)
	  printf(":%s\n", scriptFile[i]);
#endif


	/* Open device */
	fd = open(SOO_CORE_DEVICE, O_RDWR);
	if (fd < 0) {
		perror("open");
		printf("failed to open device (%d)\n", -errno);
		goto out;
	}

	do {
		/*
		 * eventd (or core_uevent) now checks for an available uevent to process.
		 * If no uevent is present, the thread is sleeping.
		 */
		found = ioctl(fd, AGENCY_IOCTL_PICK_NEXT_UEVENT, 0);
		if (found < 0) {
			printf("failed to get a ME slot (%d)\n", rc);
			goto out;
		}

		if (found) {
			/* A uevent is available... process it... */
			rc = process_uevent(fd);
			if (rc < 0)
				printf("%s: failed to process uevent, rc = %d\n", __func__, rc);
		}
	} while (found);

	out:

	close(fd);
}
