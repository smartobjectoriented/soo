/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include <linux/ctype.h>

#include <opencn/strtox.h>
#include <opencn/hal/hal.h>

ssize_t streamer_write_fp(hal_user_t *hal_user, size_t len, int num_pins, union hal_stream_data *src_data, char *cp)
{
	int n;
	union hal_stream_data *dptr;
	char *cp2;
	const char *errmsg;

	errmsg = NULL;

	for (n = 0; n < num_pins; n++) {

		dptr = &src_data[n];

		/* strip leading whitespace */
		while (isspace(*cp)) {
			cp++;
		}
		switch (hal_stream_element_type(&hal_user->stream, n)) {
			case HAL_FLOAT:
				dptr->f = strtod(cp, &cp2);
				break;

			case HAL_BIT:
				if (*cp == '0') {
					dptr->b = 0;
					cp2 = cp + 1;
				} else if (*cp == '1') {
					dptr->b = 1;
					cp2 = cp + 1;
				} else {
					errmsg = "bit value not 0 or 1";
					cp2 = cp;
				}
				break;
			case HAL_U32:
				dptr->u = strtoul(cp, &cp2, 10);
				break;

			case HAL_S32:
				dptr->s = strtol(cp, &cp2, 10);
				break;
			default:
				/* better not happen */
				goto out;
		}
		if (errmsg == NULL) {
			/* no error yet, check for other possibilties */
			/* whitespace separates fields, and there is a newline
			 at the end... so if there is not space or newline at
			 the end of a field, something is wrong. */
			if (*cp2 == '\0') {
				errmsg = "premature end of line";
				len = -1;
			} else if (!isspace(*cp2)) {
				errmsg = "bad character";
				len = -1;
			}
		}
		/* test for any error */
		if (errmsg != NULL) {
			/* abort loop on error */
			break;
		}
		/* advance pointers for next field */
		dptr++;
		cp = cp2;
	}

	if (errmsg != NULL) {
		/* print message */
		printk("%s: error in field %d: %s, skipping the line\n", __func__, n, errmsg);
		/** TODO - decide whether to skip this line and continue, or
		 abort the program.  Right now it skips the line. */
	} else {
		/* good data, keep it */
		hal_stream_wait_writable(&hal_user->stream);
		hal_stream_write(&hal_user->stream, src_data);
	}

out:
	return len;
}
