#ifndef FG_RTC_ACTOR_H
#define FG_RTC_ACTOR_H

#include "actor/fg_actor.h"

#define FG_RTC_FREQ_POWER 10
#define FG_RTC_TICKS_PER_SECOND (1U << FG_RTC_FREQ_POWER) // in Hz - currently 2^10 Hz = 1024 Hz

// RTC tick calculation (using ceil to err on the safe side):
// ticks = ceil(required fraction of a second * ticks per second)      (1)
// required fraction of a second = required time in ms / 1000 ms       (2)

// Setting (2) in (1):
// ticks = ceil((required time in ms / 1000 ms) * ticks per second)
//       = ceil((ticks per second * required time in ms) / 1000 ms)    (3)
//       = ceil((ticks per second * required time in us) / 1000000 us) (4)

// Implementing (3)
#define FG_RTC_ACTOR_1SEC_IN_MS 1000
#define FG_RTC_ACTOR_MS_TO_TICKS(REQUIRED_TIME_IN_MS)                                              \
    CEIL_DIV(FG_RTC_TICKS_PER_SECOND *(REQUIRED_TIME_IN_MS), FG_RTC_ACTOR_1SEC_IN_MS)

// Implementing (4)
#define FG_RTC_ACTOR_1SEC_IN_US 1000000
#define FG_RTC_ACTOR_US_TO_TICKS(REQUIRED_TIME_IN_US)                                              \
    CEIL_DIV(FG_RTC_TICKS_PER_SECOND *(REQUIRED_TIME_IN_US), FG_RTC_ACTOR_1SEC_IN_US)

typedef enum
{
    FG_RTC_ENABLE,
    FG_RTC_DISABLE,
    FG_RTC_START_TIMER
} fg_rtc_actor_message_code_t;

FG_ACTOR_INTERFACE_DEC(rtc, fg_rtc_actor_message_code_t);

#endif // FG_RTC_ACTOR_H__