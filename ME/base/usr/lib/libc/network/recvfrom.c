#include <sys/socket.h>
#include "syscall.h"
#include "libc.h"

ssize_t recvfrom(int fd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict addr, socklen_t *restrict alen)
{
	return sys_recvfrom(fd, buf, len, flags, addr, alen);
}
