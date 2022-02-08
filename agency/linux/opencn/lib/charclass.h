/*
 * Public domain, 2008, Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * $OpenBSD: charclass.h,v 1.1 2008/10/01 23:04:13 millert Exp $
 */
/*
 * POSIX character class support for fnmatch() and glob().
 */

#include <opencn/ctypes/ctype.h>

static struct cclass {
	const char *name;
	int (*isctype)(int);
} cclasses[] = {
	{ "alnum",	__isalnum },
	{ "alpha",	__isalpha },
	{ "blank",	isblank },
	{ "cntrl",	__iscntrl },
	{ "digit",	__isdigit },
	{ "graph",	__isgraph },
	{ "lower",	__islower },
	{ "print",	__isprint },
	{ "punct",	__ispunct },
	{ "space",	__isspace },
	{ "upper",	__isupper },
	{ "xdigit",	__isxdigit },
	{ NULL,		NULL }
};
#define NCCLASSES	(sizeof(cclasses) / sizeof(cclasses[0]) - 1)

