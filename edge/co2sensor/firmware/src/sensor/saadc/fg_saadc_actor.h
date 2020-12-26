#ifndef FG_SAADC_ACTOR_H
#define FG_SAADC_ACTOR_H

#include "actor/fg_actor.h"
#include <nrf_saadc.h>

typedef enum {
    FG_SAADC_MEASURE,
    FG_SAADC_CALIBRATE
} fg_saadc_actor_message_code_t;

typedef uint32_t fg_saadc_result_t;

FG_ACTOR_INTERFACE_DEC(saadc, fg_saadc_actor_message_code_t);

#endif // FG_SAADC_ACTOR_H__
