#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define H_RES 1024
#define V_RES 768
#define FB_SIZE (H_RES * V_RES * 4)

uint32_t create_px(uint8_t r, uint8_t g, uint8_t b);

/*
 * Demo application to show how to open a framebuffer file type, use mmap to
 * access its framebuffer and write into it.
 */
int main(int argc, char **argv)
{
	int fd;
	uint32_t i, j;
	uint32_t *fbp;

	/* Get file descriptor for /dev/fb0, i.e. the first fb device registered. */
	fd = open("/dev/fb0", O_WRONLY);

	/* Map the framebuffer memory to a process virtual address. */
	fbp = mmap(NULL, FB_SIZE, 0, 0, fd, 0);

	/* Display some pixels. */

	printf("Displaying red pixels\n");

	for (i = 0; i < V_RES / 3; i++) {
		for (j = 0; j < H_RES; j++) {
			fbp[j + i * H_RES] = create_px(0xff, 0, 0);
		}
	}

	printf("Displaying green pixels\n");
	fbp += V_RES / 3 * H_RES;
	for (i = 0; i < V_RES / 3; i++) {
		for (j = 0; j < H_RES; j++) {
			fbp[j + i * H_RES] = create_px(0, 0xff, 0);
		}
	}

	printf("Displaying blue pixels\n");
	fbp += V_RES / 3 * H_RES;

	for (i = 0; i < V_RES / 3; i++) {
		for (j = 0; j < H_RES; j++) {
			fbp[j + i * H_RES] = create_px(0, 0, 0xff);
		}
	}

	close(fd);
	return 0;
}

uint32_t create_px(uint8_t r, uint8_t g, uint8_t b)
{
	return (b << 16) | (g << 8) | r; /* 24bpp BGR mode */
}
