

#ifndef OPENCN_STRINGS_H
#define OPENCN_STRINGS_H

typedef enum {
	OPENCN_COLOR_RESET,
	OPENCN_COLOR_RED,
	OPENCN_COLOR_BRED,
	OPENCN_COLOR_GREEN,
	OPENCN_COLOR_BGREEN,
	OPENCN_COLOR_YELLOW,
	OPENCN_COLOR_BYELLOW,
	OPENCN_COLOR_BLUE,
	OPENCN_COLOR_BBLUE,
	OPENCN_COLOR_MAGENTA,
	OPENCN_COLOR_BMAGENTA,
	OPENCN_COLOR_CYAN,
	OPENCN_COLOR_BCYAN,
	OPENCN_COLOR__COUNT
} OPENCN_COLOR;


int opencn_printf(const char *fmt, ...);
int opencn_snprintf(char *buf, size_t size, const char *fmt, ...);
int opencn_cprintf(OPENCN_COLOR color, const char* fmt, ...);
int fnmatch(const char *pattern, const char *string, int flags);


#endif /* OPENCN_STRINGS_H */
