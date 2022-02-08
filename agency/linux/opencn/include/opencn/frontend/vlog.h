
#ifndef VLOG_FRONT_H
#define VLOG_FRONT_H

#include <linux/types.h>

extern bool vlog_enabled;

void vlog_init(void);
void vlog_send(char *line);

#endif /* VLOG_FRONT_H */
