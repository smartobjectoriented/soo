/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2019 Baptiste Delporte <bonel@bonel.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <soo/sooenv.h>

#include <soo/hypervisor.h>

#include <soo/uapi/console.h>

/* Agency Core */
static bool log_soo_core = false;

/* DCM */
static bool log_soo_dcm = false;

/* SOOlink */
static bool log_soo_soolink = false;

/* Discovery */
static bool log_soo_soolink_discovery = false;

/* Transcoder */
static bool log_soo_soolink_transcoder = false;
static bool log_soo_soolink_transcoder_block = false;

/* Winenet */
static bool log_soo_soolink_winenet = false;
static bool log_soo_soolink_winenet_beacon = false;
static bool log_soo_soolink_winenet_neighbour = false;
static bool log_soo_soolink_winenet_state = false;
static bool log_soo_soolink_winenet_state_idle = false;

static bool log_soo_soolink_winenet_ping = false;
static bool log_soo_soolink_winenet_ack = false;

static bool log_soo_soolink_plugin = false;

/* Backends */
static bool log_soo_backend_vsenseled = true;
static bool log_soo_backend_vuihandler = true;

struct mutex soo_log_lock;

bool __soo_log_lock_initialized = false;

extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

void (*__printch)(char c) = NULL;

void __lprintk(const char *format, va_list va) {
	char buf[CONSOLEIO_BUFFER_SIZE];
	char *__start;
	int i;

	vsnprintf(buf, CONSOLEIO_BUFFER_SIZE, format, va);

	__start = buf;

	/* Skip printk prefix issued by the standard Linux printk if any */
	if ((*__start != 0) && (*__start < 10))
		__start += 2;

	for (i = 0; i < strlen(__start); i++)
		if (likely(__printch))
			__printch(__start[i]);

}

void lprintch(char c) {
	if (likely(__printch))
		__printch(c);
}

void lprintk(char *format, ...) {

	va_list va;
	va_start(va, format);

	__lprintk(format, va);

	va_end(va);
}

void __soo_log(char *info, char *buf) {
	char prefix[50];
	static char __internal_buf[CONSOLEIO_BUFFER_SIZE] = { };
	int i;
#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
	int j;
#endif
	bool outlog = false;
	static bool force_log = false;

	if (__internal_buf[0] == 0) {

		if ((buf[0] == '*') && (buf[1] == '*') && (buf[2] == '*'))
			force_log = true;

#ifdef CONFIG_SOOLINK_PLUGIN_SIMULATION
		/* Make a friendly indentation according to the SOO number */
		sscanf(current_soo->name, "SOO-%d", &i);

		for (j = 0; j < (i-1)*8; j++)
			strcat(__internal_buf, " ");

#endif
		/* Add log information */
		sprintf(prefix, "(%s) ", info);
		strcat(__internal_buf, prefix);
	}

	strcat(__internal_buf, buf);

	if (buf[strlen(buf)-1] != '\n')
		return ;

	/* Agency Core */
	if ((log_soo_core && (strstr(__internal_buf, "[soo:core"))))
		outlog = true;

	/* DCM */
	if ((log_soo_dcm && (strstr(__internal_buf, "[soo:dcm"))))
		outlog = true;

	/* SOOlink overall logs */
	if (log_soo_soolink && (strstr(__internal_buf, "[soo:soolink")))
		outlog = true;

	/* SOOlink Discovery functional block */
	if (log_soo_soolink_discovery && (strstr(__internal_buf, "[soo:soolink:discovery")))
		outlog = true;

	/* SOOlink Transcoder functional block */
	if ((log_soo_soolink_transcoder && (strstr(__internal_buf, "[soo:soolink:transcoder"))) ||
	    (log_soo_soolink_transcoder_block && (strstr(__internal_buf, "[soo:soolink:transcoder:block"))))
		outlog = true;

	/* SOOlink Winenet protocol */
	if ((log_soo_soolink_winenet && (strstr(__internal_buf, "[soo:soolink:winenet"))) ||
	    (log_soo_soolink_winenet_state && (strstr(__internal_buf, "[soo:soolink:winenet:state"))) ||
	    (log_soo_soolink_winenet_state_idle && (strstr(__internal_buf, "[soo:soolink:winenet:state:idle"))) ||
	    (log_soo_soolink_winenet_neighbour && (strstr(__internal_buf, "[soo:soolink:winenet:neighbour"))) ||
	    (log_soo_soolink_winenet_ack && (strstr(__internal_buf, "[soo:soolink:winenet:ack"))) ||
	    (log_soo_soolink_winenet_ping && (strstr(__internal_buf, "[soo:soolink:winenet:ping"))) ||
	    (log_soo_soolink_winenet_beacon && (strstr(__internal_buf, "[soo:soolink:winenet:beacon"))) ||
	    (log_soo_soolink_plugin && (strstr(__internal_buf, "[soo:soolink:plugin")))
	    )
		outlog = true;

	/* Backends */
	if ((log_soo_backend_vsenseled && (strstr(__internal_buf, "[soo:backend:vsenseled"))) ||
	    (log_soo_backend_vuihandler && (strstr(__internal_buf, "[soo:backend:vuihandler")))
	    )
		outlog = true;

	/* Print out to the console */
	if (!outlog && !force_log) {
		__internal_buf[0] = 0;
		return ;
	}
	force_log = false;

	/* Out to the interface...*/

	for (i = 0; i < strlen(__internal_buf); i++)
		if (likely(__printch))
			__printch(__internal_buf[i]);

	__internal_buf[0] = 0;
}

void soo_log(char *format, ...) {
	va_list va;
	char buf[CONSOLEIO_BUFFER_SIZE];

	if (unlikely(!__soo_log_lock_initialized)) {
		mutex_init(&soo_log_lock);
		__soo_log_lock_initialized = true;
	}

	mutex_lock(&soo_log_lock);

	va_start(va, format);
	vsnprintf(buf, CONSOLEIO_BUFFER_SIZE, format, va);
	va_end(va);

	__soo_log(current_soo->name, buf);

	mutex_unlock(&soo_log_lock);
}

/**
 * Print the contents of a buffer.
 */
void soo_log_buffer(void *buffer, uint32_t n) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		soo_log("%02x ", ((char *) buffer)[i]);
}

/**
 * Print the contents of a buffer.
 */
void lprintk_buffer(void *buffer, uint32_t n) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		lprintk("%02x ", ((char *) buffer)[i]);
}

/**
 * Print the contents of a buffer.
 */
void printk_buffer(void *buffer, uint32_t n) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		pr_cont("%02x ", ((char *) buffer)[i]);
}

/**
 * Print the contents of a buffer. Each element is separated using a given character.
 */
void lprintk_buffer_separator(void *buffer, uint32_t n, char separator) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		lprintk("%02x%c", ((char *) buffer)[i], separator);
	lprintk("\n");
}

/**
 * Print an uint64_t number and concatenate a string.
 */
void lprintk_int64_post(s64 number, char *post) {
	uint32_t msb = number >> 32;
	uint32_t lsb = number & 0xffffffff;

	lprintk("%08x %08x%s", msb, lsb, post);
}

/**
 * Print an uint64_t number.
 */
void lprintk_int64(s64 number) {
	lprintk_int64_post(number, "\n");
}
