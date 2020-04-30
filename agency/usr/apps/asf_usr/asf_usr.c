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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include<stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ASF_MAX_PRINT_SIZE	256

#define ASF_TA_DEV_NAME  	"/dev/soo/asf"

#define ASF_TA_PATH			"/root/ta/"
#define ASF_TA_EXTENSION	"ta"

#define ASF_IMSG(fmt, ...)   asf_print(__func__, __LINE__, "ASF", "INFO", fmt, ##__VA_ARGS__)
#define ASF_EMSG(fmt, ...)   asf_print(__func__, __LINE__, "ASF", "ERROR", fmt, ##__VA_ARGS__)


/**
 * asf_print() - ASF raw print/log function.
 *
 * It should not be used directly. Use ASF_EMSG or ASF_IMSG macro
 */
static void asf_print(const char *function, int line, const char *prefix, char *level, const char *fmt, ...)
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

/**
 * read_ta_file() - Read a TA binary file and store the result in 'buf'
 *
 * Returns the size of the TA
 */
static size_t read_ta_file(const char *ta, void **buf)
{
	FILE *file = NULL;
	size_t size = 0;

	ASF_IMSG("Read '%s' TA file\n", ta);

	file = fopen(ta, "rb");
	if (!file) {
		ASF_EMSG("opening '%s' file failed\n", __func__, ta);
		goto read_ta_failed;
	}

	if (fseek(file, 0, SEEK_END)) {
		ASF_EMSG("seek file failed\n");
		goto read_ta_close;
	}

	size = ftell(file);
	rewind(file);

	*buf = malloc(size);
	if (!*buf) {
		ASF_EMSG("Malloc failed\n");
		goto read_ta_close;
	}

	if (fread(*buf, 1, size, file) != size) {
		ASF_EMSG("Read TA '%s' file failed\n", ta);
		goto read_ta_free;
	}

	fclose(file);

	return size;

read_ta_free:
	free(buf);
read_ta_close:
	fclose(file);
read_ta_failed:
	return -1;
}

/**
 * write_ta() - Send the TA to ASF kernel to be written is Optee Secure Storage
 */
static bool write_ta(void *buf, size_t size)
{
	int fd;

	ASF_IMSG("Write TA\n");

	fd = open(ASF_TA_DEV_NAME, O_RDWR);
	if (!fd) {
		ASF_EMSG("Opening '%s' failed\n", ASF_TA_DEV_NAME);
		goto write_ta_error;
	}

	if (write(fd, buf,size) != size) {
		ASF_EMSG("Write TA failed\n", ASF_TA_DEV_NAME);
		goto write_ta_failed;
	}

	close(fd);

	return true;

write_ta_failed:
	close(fd);
write_ta_error:
	return false;

}

/**
 * install_ta() - Start the procedure to install a TA in Optee Secure Storage
 *
 * NB: It is ASF in kernel which communicates with Optee to perform The installation
 *     of the TA in the Secure Storage. This function only reads the TA file and
 *     sends it to ASF kernel
 */
static bool install_ta(const char *ta)
{
	void *buf = NULL;
	int fd;
	bool ret = true;
	size_t size = 0;

	ASF_IMSG("Installation of '%s' TA\n", ta);

	/* 1. Read TA */
	size = read_ta_file(ta, &buf);
	if (size < 0)
		return false;

	/* 2. Write TA */
	ret = write_ta(buf, size);

	free(buf);

	return ret;
}

/**
 * erase_ta() - Secure erase of the TA file. It should be performed after TA installation
 */
static void erase_ta(const char *ta)
{
	char cmd_shred[100] = "shred ";
	char cmd_rm[100] = "rm ";

	ASF_IMSG("Erase '%s' TA\n", ta);

	strncat(cmd_shred, ta, strlen(ta));
	strncat(cmd_rm, ta, strlen(ta));

	printf("%s\n", cmd_shred);
	printf("%s\n", cmd_rm);

	system(cmd_shred);
	system(cmd_rm);
}

/**
 * ends_with() - check if a string ends with a certain extension
 *
 * It is used to check the extension name of the TA file.
 */
static bool ends_with(const char *str, const char *ext)
{
    size_t str_len;
    size_t ext_len;

    if (!str || !ext)
        return 0;

	str_len = strlen(str);
	ext_len = strlen(ext);

    if (ext_len >  str_len)
        return false;
    return strncmp(str + str_len - ext_len, ext, ext_len) == 0;
}

/**
 * is_ta() - Checks if a file corresponds to a TA file name
 *
 * It is used to check the extension name of the TA file. The name  format of a
 * TA is uuid.ta. For example:  8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta
 */
static bool is_ta(const char *file_name)
{
	bool ret;

	/* A TA binary ends with 'ta' extension */
	ret = ends_with(file_name, ASF_TA_EXTENSION);

	/* TODO - add check of the TA name which should be a uuid */
#warning  add check of the TA name which should corresponds to an uuid

	return ret;
}


int main(int argc, char *argv[])
{
	// size_t ret = 0;
	bool ret;
	int mode;
	DIR *d;
	struct dirent *dir;
	char ta_path[100] = ASF_TA_PATH;

	ASF_IMSG("ASF User Space app\n");

	d = opendir(ASF_TA_PATH);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if ((dir->d_type == DT_REG) && (is_ta(dir->d_name))) {
				strncat(ta_path, dir->d_name, strlen(dir->d_name));

				ret = install_ta(ta_path);
				/* TODO what to if installation failed ? */
				if (!ret)
					ASF_EMSG("TA installation failed\n");

				/* Erase the TA in all cases to avoid this TA To remains in */
				/* the Linux file system */
				erase_ta(ta_path);
			}

		}
		closedir(d);

  	}

	return 0;
}
