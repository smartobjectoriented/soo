#if !defined(LCCT_JOG_H)
#define LCCT_JOG_H

#include "lcct_internal.h"

FSM_STATUS lcct_jog(void);
void lcct_jog_reset(void);
int lcct_jog_init(int comp_id);
int lcct_jog_is_relative(void);

#endif // LCCT_JOG_H
