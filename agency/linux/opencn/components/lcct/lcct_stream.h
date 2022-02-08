#if !defined(LCCT_STREAM_H)
#define LCCT_STREAM_H

#include "lcct_internal.h"

FSM_STATUS lcct_stream(void);
void lcct_stream_reset(void);
int lcct_stream_init(int comp_id);
void lcct_stream_stop(void);

#endif // LCCT_STREAM_H
