
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// #define TA_PATH				"8aaaf200-2450-11e4-abe2-0002a5d5c51b.stripped.elf"
#define TA_PATH				"8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
#define ASF_TA_DEV_NAME  	"/dev/soo/asf"

#define ASF_TA_IOCTL_HELLO_WORLD		0

#define ASF_TA_MODE_INSTALL		0
#define ASF_TA_MODE_HELLO_WORLD	1

static void install_ta(void)
{
	FILE *file = NULL;
	void *buf = NULL;
	size_t size = 0;
	int fd;

	/* 1. Read TA */
	file = fopen(TA_PATH, "rb");
	if (!file)
		printf("fopen failed\n");

	if (fseek(file, 0, SEEK_END))
		printf("fseek failed\n");

	size = ftell(file);
	rewind(file);

	buf = malloc(size);
	if (!buf)
		printf("malloc failed\n");

	if (fread(buf, 1, size, file) != size)
		printf("fread failed\n");

	fclose(file);

	/* 2. Write TA */
	fd = open(ASF_TA_DEV_NAME, O_RDWR);
	if (!fd) {
		printf("fopen failed\n");
	}

	if (write(fd, buf,size) != size)
		printf("fwrite failed\n");

	close(fd);
}


static void hello_world_cmd(void)
{
	int fd;

	fd = open(ASF_TA_DEV_NAME, O_RDWR);
	if (!fd) {
		printf("fopen failed\n");
	}

	ioctl(fd, ASF_TA_IOCTL_HELLO_WORLD, NULL);

	close(fd);
}

int main(int argc, char *argv[])
{
	size_t ret = 0;
	int mode;

	printf("== ASF User space APP\n");

	if (argc > 2) {
		printf("ERROR wrong number of parameters\n");
		return -1;

	} else if (argc == 2) {
		printf("MODE: install\n");
		mode = ASF_TA_MODE_INSTALL;
	} else {
		printf("MODE: Hello World\n");
		mode = ASF_TA_MODE_HELLO_WORLD;
	}

	switch (mode) {
		case ASF_TA_MODE_INSTALL:
			install_ta();
			break;
		case ASF_TA_MODE_HELLO_WORLD:
			hello_world_cmd();
			break;
		default:
			printf("ERROR - mode '%d' not supported\n", mode);
			return -1;
	}

	return 0;
}
