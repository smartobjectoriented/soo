#ifndef LCCT_HOME_H
#define LCCT_HOME_H

#include <opencn/hal/hal.h> 
#include "lcct_internal.h"


void lcct_home_reset(void);
int lcct_home_init(int comp_id);
FSM_STATUS lcct_home(void);
int lcct_is_homed(void);

#endif