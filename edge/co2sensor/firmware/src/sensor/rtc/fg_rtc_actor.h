#ifndef FG_RTC_ACTOR_H
#define FG_RTC_ACTOR_H

#include "actor/fg_actor.h"

typedef enum
{
    FG_RTC_START_TIMER
} fg_rtc_actor_message_code_t;

FG_ACTOR_INTERFACE_DEC(rtc, fg_rtc_actor_message_code_t);

#endif // FG_RTC_ACTOR_H__