#include <unistd.h>
#include <fcntl.h>
#include "syscall.h"

int unlink(char *path)
{
	return sys_unlink(path);
}
