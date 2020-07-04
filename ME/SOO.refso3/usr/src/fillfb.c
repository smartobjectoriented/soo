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
void display_line(uint32_t *fbp, uint32_t start, uint32_t px);

/*
 * Demo application to show how to open a framebuffer file type, use mmap to
 * access its framebuffer and write into it.
 */
int main(int argc, char **argv)
{
	int fd;
	uint32_t i, px;
	uint32_t *fbp;

	uint32_t colors[] = {
		create_px(0xff, 0, 0),
		create_px(0, 0xff, 0),
		create_px(0, 0, 0xff)
	};

	/* Get file descriptor for /dev/fb0, i.e. the first fb device registered. */
	fd = open("/dev/fb0", O_WRONLY);

	/* Map the framebuffer memory to a process virtual address. */
	fbp = mmap(NULL, FB_SIZE, 0, 0, fd, 0);

	/* Display lines of different colors. */
	for (i = 0; i < V_RES; i++) {
		px = colors[(i / 12) % 3];
		display_line(fbp, i * H_RES, px);
	}

	close(fd);
	return EXIT_SUCCESS;
}

void display_line(uint32_t *fbp, uint32_t start, uint32_t px)
{
	uint32_t i;
	for (i = 0; i < H_RES; i++) {
		fbp[start + i] = px;
	}
}

uint32_t create_px(uint8_t r, uint8_t g, uint8_t b)
{
	return (b << 16) | (g << 8) | r; /* 24bpp BGR mode */
}
