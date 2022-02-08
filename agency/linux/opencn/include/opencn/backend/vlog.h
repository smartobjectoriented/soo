
#ifndef VLOG_BACK_H
#define VLOG_BACK_H

#include <opencn/dev/vlog.h>

void probe_vlogback(vlog_sring_t *sring, uint32_t evtchn);

void vlog_do_flush(void);

#endif /* VLOG_BACK_H */
