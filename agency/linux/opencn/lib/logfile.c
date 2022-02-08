/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/syscalls.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/fsnotify.h>

#include <uapi/linux/stat.h>

#include <uapi/asm/fcntl.h>

#include <opencn/logfile.h>

#include <opencn/backend/vlog.h>

struct file *logfile_filp;
bool logfile_ready = false;


/*************/

bool logfile_enabled(void) {
	return logfile_ready;
}

void logfile_write(char *s) {
	loff_t pos = logfile_filp->f_pos;

	kernel_write(logfile_filp, s, strlen(s) + 1, &pos);

	logfile_filp->f_pos = pos;

}

void logfile_close(void) {

#ifdef CONFIG_X86
	vlog_do_flush();
#endif

	filp_close(logfile_filp, NULL);

	/* Perform the sync with the storage. */
	vfs_fsync(logfile_filp, 0);

	logfile_ready = false;
}

void logfile_init(void) {

	logfile_filp = filp_open("/var/log/opencn.log",  O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

	/* Increment the reference count on this filp (used by close). */
	get_file(logfile_filp);

	logfile_ready = true;

}
