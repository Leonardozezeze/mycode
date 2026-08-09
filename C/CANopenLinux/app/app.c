/*
 * User application code for CANopenLinux.
 *
 * Implements the application hooks declared in CO_application.h, which are
 * called from CO_main_basic.c when built with -DCO_USE_APPLICATION.
 *
 * Exchange process data with the CANopen network through the Object
 * Dictionary variables in OD.h (mapped to PDOs or accessed via SDO).
 */

#include <stdio.h>

#include "CO_application.h"
#include "OD.h"

/* Called once on program startup, after OD initialization and before CANopen
 * initialization. bitRate/nodeId may be overridden here. */
CO_ReturnError_t
app_programStart(uint16_t* bitRate, uint8_t* nodeId, uint32_t* errInfo) {
    (void)bitRate;
    (void)nodeId;
    (void)errInfo;

    return CO_ERROR_NO;
}

/* Called after CANopen communication reset. */
void
app_communicationReset(CO_t* co) {
    (void)co;
}

/* Called just before program ends. */
void
app_programEnd(void) {
}

/* Called cyclically from the main loop (default every 100 ms).
 * Place slower, non-blocking logic here. When accessing OD variables that are
 * also accessed from app_programRt() or PDOs, protect them with
 * CO_LOCK_OD(CO->CANmodule) / CO_UNLOCK_OD(CO->CANmodule). */
void
app_programAsync(CO_t* co, uint32_t timer1usDiff) {
    (void)co;
    (void)timer1usDiff;
}

/* Called cyclically from the realtime processing (default every 1 ms).
 * Code here must execute fast. */
void
app_programRt(CO_t* co, uint32_t timer1usDiff) {
    (void)co;
    (void)timer1usDiff;
}
