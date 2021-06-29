
#include <linux/ctype.h>

#include <soo/ctype.h>

int __isalpha(int c)
{
	return isalpha(c);
}

int __isdigit(int c)
{
	return isdigit(c);
}

int __isalnum(int c)
{
	return isalnum(c);
}

int isblank(int c)
{
	return (c == ' ' || c == '\t');
}

int __iscntrl(int c)
{
	return iscntrl(c);
}

int __isgraph(int c)
{
	return isgraph(c);
}

int __islower(int c)
{
	return islower(c);
}

int __isprint(int c)
{
	return isprint(c);
}

int __ispunct(int c)
{
	return ispunct(c);
}

int __isspace(int c)
{
	return isspace(c);
}

int __isupper(int c)
{
	return isupper(c);
}

int __isxdigit(int c)
{
	return isxdigit(c);
}

