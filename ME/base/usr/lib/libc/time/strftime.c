#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <langinfo.h>
#include <locale.h>
#include <time.h>
#include <limits.h>
#include <libc.h>

#include "locale_impl.h"
#include "time_impl.h"

const char *__nl_langinfo_l(nl_item n, locale_t l) {

	switch (n) {

	case ABDAY_1:
		return "Sun";
	case ABDAY_2:
		return "Mon";
	case ABDAY_3:
		return "Tue";
	case ABDAY_4:
		return "Wed";
	case ABDAY_5:
		return "Thu";
	case ABDAY_6:
		return "Fri";
	case ABDAY_7:
		return "Sat";

	case ABMON_1:
		return "Jan";
	case ABMON_2:
		return "Feb";
	case ABMON_3:
		return "Mar";
	case ABMON_4:
		return "Apr";
	case ABMON_5:
		return "May";
	case ABMON_6:
		return "Jun";
	case ABMON_7:
		return "Jul";
	case ABMON_8:
		return "Aug";
	case ABMON_9:
		return "Sep";
	case ABMON_10:
		return "Oct";
	case ABMON_11:
		return "Nov";
	case ABMON_12:
		return "Dec";

	default:
		return "(n/a)";
	}

}

static int is_leap(int y)
{
	/* Avoid overflow */
	if (y>INT_MAX-1900) y -= 2000;
	y += 1900;
	return !(y%4) && ((y%100) || !(y%400));
}

static int week_num(const struct tm *tm)
{
	int val = (tm->tm_yday + 7U - (tm->tm_wday+6U)%7) / 7;
	/* If 1 Jan is just 1-3 days past Monday,
	 * the previous week is also in this year. */
	if ((tm->tm_wday + 371U - tm->tm_yday - 2) % 7 <= 2)
		val++;
	if (!val) {
		val = 52;
		/* If 31 December of prev year a Thursday,
		 * or Friday of a leap year, then the
		 * prev year has 53 weeks. */
		int dec31 = (tm->tm_wday + 7U - tm->tm_yday - 1) % 7;
		if (dec31 == 4 || (dec31 == 5 && is_leap(tm->tm_year%400-1)))
			val++;
	} else if (val == 53) {
		/* If 1 January is not a Thursday, and not
		 * a Wednesday of a leap year, then this
		 * year has only 52 weeks. */
		int jan1 = (tm->tm_wday + 371U - tm->tm_yday) % 7;
		if (jan1 != 4 && (jan1 != 3 || !is_leap(tm->tm_year)))
			val = 1;
	}
	return val;
}

const char *__tm_to_tzname(const struct tm *);
size_t __strftime_l(char *restrict, size_t, const char *restrict, const struct tm *restrict, locale_t);

const char *__strftime_fmt_1(char (*s)[100], size_t *l, int f, const struct tm *tm, locale_t loc)
{
	nl_item item;
	long long val;
	const char *fmt = "-";
	int width = 2;

	switch (f) {
	case 'a':
		if (tm->tm_wday > 6U) goto string;
		item = ABDAY_1 + tm->tm_wday;
		goto nl_strcat;
	case 'A':
		if (tm->tm_wday > 6U) goto string;
		item = DAY_1 + tm->tm_wday;
		goto nl_strcat;
	case 'h':
	case 'b':
		if (tm->tm_mon > 11U) goto string;
		item = ABMON_1 + tm->tm_mon;
		goto nl_strcat;
	case 'B':
		if (tm->tm_mon > 11U) goto string;
		item = MON_1 + tm->tm_mon;
		goto nl_strcat;
	case 'c':
		item = D_T_FMT;
		goto nl_strftime;
	case 'C':
		val = (1900LL+tm->tm_year) / 100;
		goto number;
	case 'd':
		val = tm->tm_mday;
		goto number;
	case 'D':
		fmt = "%m/%d/%y";
		goto recu_strftime;
	case 'e':
		*l = snprintf(*s, sizeof *s, "%2d", tm->tm_mday);
		return *s;
	case 'F':
		fmt = "%Y-%m-%d";
		goto recu_strftime;
	case 'g':
	case 'G':
		val = tm->tm_year + 1900LL;
		if (tm->tm_yday < 3 && week_num(tm) != 1) val--;
		else if (tm->tm_yday > 360 && week_num(tm) == 1) val++;
		if (f=='g') val %= 100;
		else width = 4;
		goto number;
	case 'H':
		val = tm->tm_hour;
		goto number;
	case 'I':
		val = tm->tm_hour;
		if (!val) val = 12;
		else if (val > 12) val -= 12;
		goto number;
	case 'j':
		val = tm->tm_yday+1;
		width = 3;
		goto number;
	case 'm':
		val = tm->tm_mon+1;
		goto number;
	case 'M':
		val = tm->tm_min;
		goto number;
	case 'n':
		*l = 1;
		return "\n";
	case 'p':
		item = tm->tm_hour >= 12 ? PM_STR : AM_STR;
		goto nl_strcat;
	case 'r':
		item = T_FMT_AMPM;
		goto nl_strftime;
	case 'R':
		fmt = "%H:%M";
		goto recu_strftime;
	case 's':
		val = __tm_to_secs(tm) - tm->__tm_gmtoff;
		width = 1;
		goto number;
	case 'S':
		val = tm->tm_sec;
		goto number;
	case 't':
		*l = 1;
		return "\t";
	case 'T':
		fmt = "%H:%M:%S";
		goto recu_strftime;
	case 'u':
		val = tm->tm_wday ? tm->tm_wday : 7;
		width = 1;
		goto number;
	case 'U':
		val = (tm->tm_yday + 7U - tm->tm_wday) / 7;
		goto number;
	case 'W':
		val = (tm->tm_yday + 7U - (tm->tm_wday+6U)%7) / 7;
		goto number;
	case 'V':
		val = week_num(tm);
		goto number;
	case 'w':
		val = tm->tm_wday;
		width = 1;
		goto number;
	case 'x':
		item = D_FMT;
		goto nl_strftime;
	case 'X':
		item = T_FMT;
		goto nl_strftime;
	case 'y':
		val = (tm->tm_year + 1900LL) % 100;
		if (val < 0) val = -val;
		goto number;
	case 'Y':
		val = tm->tm_year + 1900LL;
		if (val >= 10000) {
			*l = snprintf(*s, sizeof *s, "+%lld", val);
			return *s;
		}
		width = 4;
		goto number;
	case 'z':
		if (tm->tm_isdst < 0) {
			*l = 0;
			return "";
		}
		*l = snprintf(*s, sizeof *s, "%+.2d%.2d",
			(tm->__tm_gmtoff)/3600,
			abs(tm->__tm_gmtoff%3600)/60);
		return *s;
	case 'Z':
		if (tm->tm_isdst < 0) {
			*l = 0;
			return "";
		}
#if 0 /* so3 - Not implemented */
		fmt = __tm_to_tzname(tm);
#endif
		fmt = "";
		goto string;
	case '%':
		*l = 1;
		return "%";
	default:
		return 0;
	}
number:
	*l = snprintf(*s, sizeof *s, "%0*lld", width, val);
	return *s;
nl_strcat:
	fmt = __nl_langinfo_l(item, loc);
string:
	*l = strlen(fmt);
	return fmt;
nl_strftime:
	fmt = __nl_langinfo_l(item, loc);
recu_strftime:
	*l = __strftime_l(*s, sizeof *s, fmt, tm, loc);
	if (!*l) return 0;
	return *s;
}

size_t __strftime_l(char *restrict s, size_t n, const char *restrict f, const struct tm *restrict tm, locale_t loc)
{
	size_t l, k;
	char buf[100];
	char *p;
	const char *t;
	int plus;
	unsigned long width;
	for (l=0; l<n; f++) {
		if (!*f) {
			s[l] = 0;
			return l;
		}
		if (*f != '%') {
			s[l++] = *f;
			continue;
		}
		f++;
		if ((plus = (*f == '+'))) f++;
		width = strtoul(f, &p, 10);
		if (*p == 'C' || *p == 'F' || *p == 'G' || *p == 'Y') {
			if (!width && p!=f) width = 1;
		} else {
			width = 0;
		}
		f = p;
		if (*f == 'E' || *f == 'O') f++;
		t = __strftime_fmt_1(&buf, &k, *f, tm, loc);
		if (!t) break;
		if (width) {
			for (; *t=='+' || *t=='-' || (*t=='0'&&t[1]); t++, k--);
			width--;
			if (plus && tm->tm_year >= 10000-1900)
				s[l++] = '+';
			else if (tm->tm_year < -1900)
				s[l++] = '-';
			else
				width++;
			for (; width > k && l < n; width--)
				s[l++] = '0';
		}
		if (k > n-l) k = n-l;
		memcpy(s+l, t, k);
		l += k;
	}
	if (n) {
		if (l==n) l=n-1;
		s[l] = 0;
	}
	return 0;
}

size_t strftime(char *restrict s, size_t n, const char *restrict f, const struct tm *restrict tm)
{
	return __strftime_l(s, n, f, tm, C_LOCALE);
}

weak_alias(__strftime_l, strftime_l);
