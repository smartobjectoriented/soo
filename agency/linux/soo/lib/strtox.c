// SPDX-License-Identifier: GPL-2.0
/*
 * Convert integer string representation to an integer.
 * If an integer doesn't fit into specified type, -E is returned.
 *
 * Integer starts with optional sign.
 * strtou*() functions do not accept sign "-".
 *
 * Radix 0 means autodetection: leading "0x" implies radix 16,
 * leading "0" implies radix 8, otherwise radix is 10.
 * Autodetection hints work after optional sign, but not before.
 *
 * If -E is returned, result is not touched.
 */
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/limits.h>


/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	const char *s;
	unsigned long acc, cutoff;
	int c;
	int neg, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	cutoff = ULONG_MAX / (unsigned long)base;
	cutlim = ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (acc > cutoff || (acc == cutoff && c > cutlim)) {
			any = -1;
			acc = ULONG_MAX;
			/* errno = ERANGE; */
		} else {
			any = 1;
			acc *= (unsigned long)base;
			acc += c;
		}
	}
	if (neg && any > 0)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long
strtol(const char *nptr, char **endptr, int base)
{
	const char *s;
	long acc, cutoff;
	int c;
	int neg, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? LONG_MIN : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	if (neg) {
		if (cutlim > 0) {
			cutlim -= base;
			cutoff += 1;
		}
		cutlim = -cutlim;
	}
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (neg) {
			if (acc < cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = LONG_MIN;
				/* errno = ERANGE; */
			} else {
				any = 1;
				acc *= base;
				acc -= c;
			}
		} else {
			if (acc > cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = LONG_MAX;
				/* errno = ERANGE; */
			} else {
				any = 1;
				acc *= base;
				acc += c;
			}
		}
	}
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}

#if 0 /* Not used at the moment */

int strtou16(const char *s, unsigned int base, u16 *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(u16)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}
EXPORT_SYMBOL(strtou16);

int strtos16(const char *s, unsigned int base, s16 *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(s16)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}
EXPORT_SYMBOL(kstrtos16);

int strtou8(const char *s, unsigned int base, u8 *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(u8)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}
EXPORT_SYMBOL(kstrtou8);

int strtos8(const char *s, unsigned int base, s8 *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(s8)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}
EXPORT_SYMBOL(strtos8);

/**
 * kstrtobool - convert common user inputs into boolean values
 * @s: input strin
 * @res: result
 *
 * This routine returns 0 iff the first character is one of 'Yy1Nn0', or
 * [oO][NnFf] for "on" and "off". Otherwise it will return -EINVAL.  Value
 * pointed to by res is updated upon finding a match.
 */
int strtobool(const char *s, bool *res)
{
	if (!s)
		return -EINVAL;

	switch (s[0]) {
	case 'y':
	case 'Y':
	case '1':
		*res = true;
		return 0;
	case 'n':
	case 'N':
	case '0':
		*res = false;
		return 0;
	case 'o':
	case 'O':
		switch (s[1]) {
		case 'n':
		case 'N':
			*res = true;
			return 0;
		case 'f':
		case 'F':
			*res = false;
			return 0;
		default:
			break;
		}
	default:
		break;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(strtobool);

#endif

