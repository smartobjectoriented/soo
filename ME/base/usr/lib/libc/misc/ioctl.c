#include <sys/ioctl.h>
#include <stdarg.h>
#include <syscall.h>

int ioctl(int fd, int req, ...)
{
	void *arg;
	va_list ap;
	va_start(ap, req);
	arg = va_arg(ap, void *);
	va_end(ap);
	return sys_ioctl(fd, req, arg);
#if 0
	return syscall(SYS_ioctl, fd, req, arg);
#endif
}
