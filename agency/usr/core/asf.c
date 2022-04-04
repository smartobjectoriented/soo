/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include <assert.h>

#include "asf_priv.h"

/**
 * asf_print() - ASF raw print/log function.
 *
 * It should not be used directly. Use ASF_EMSG or ASF_IMSG macro
 */
void asf_print(const char *function, int line, const char *prefix, char *level, const char *fmt, ...)
{
	char msg[ASF_MAX_PRINT_SIZE];
	int n = 0;
	va_list ap;

	n = snprintf(msg, sizeof(msg), "> %s %s/%s:%d: ", prefix, level, function, line);
	if (n < 0)
		return;

	if ((size_t)n < sizeof(msg)) {
		va_start(ap, fmt);
		n = vsnprintf(msg + n, sizeof(msg) - n, fmt, ap);
		va_end(ap);
		if (n < 0)
			return;
	}

	fprintf(stdout, "%s", msg);
}


static void asf_open_asf_ta_session(void)
{
	int fd;
	int res;
	bool opened;

	ASF_IMSG("Opening ASF session\n");

	fd = open(ASF_DEV_NAME, O_RDWR);
	if (!fd) {
		ASF_EMSG("ASF session failed to open\n");
		return;
	}

	opened = ioctl(fd, ASF_IOCTL_SESSION_OPENED, NULL);
	if (!opened) {
		res = ioctl(fd, ASF_IOCTL_OPEN_SESSION, NULL);
		if (res < 0) {
			ASF_EMSG("ASF session failed to open\n");
			assert(false);
		}
	}

	close(fd);
}

#if 0 /* It is not used in the current SOO framework - added it to be complete */
static void asf_close_asf_ta_session(void)
{
	int devfd;

	ASF_IMSG("Clossing ASF session\n");

	devfd = open(ASF_DEV_NAME, O_RDWR);
	if (!devfd) {
		printf("Opening '%s' failed\n", ASF_DEV_NAME);
		return;
	}

	ioctl(devfd, ASF_IOCTL_CLOSE_SESSION, NULL);

	close(devfd);
}
#endif

void asf_init(void)
{
	/* Installation of the new version of the TAs */
	asf_ta_installation();

	/* Open ASF TA session if not already opened */
	asf_open_asf_ta_session();
}
