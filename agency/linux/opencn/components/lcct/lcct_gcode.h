/**
  * @file lcct_gcode.h
  * @brief LCCT submodule that handles GCode interpretation
  */

#if !defined(LCCT_GCODE_H)
#define LCCT_GCODE_H

#include "lcct_internal.h"

/**
 * @brief Calls the submodule state machine
 * @return State machine exit state
 */
FSM_STATUS lcct_gcode(void);

/**
 * @brief Force the reset on the submodule state machine
 */
void lcct_gcode_reset(void);

/**
 * @brief Initializes this submodule
 * @param comp_id: HAL component id, should be the same as LCCT
 * @return Non-zero if an error occurs
 * @attention This must be called first before anything else from this module
 */
int lcct_gcode_init(int comp_id);

/**
 * @brief Reset to US and RT feedopt modules
 */
void send_reset(void);

#endif /* LCCT_GCODE_H */
