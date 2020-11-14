#ifndef FG_LP8_ACTOR_H__
#define FG_LP8_ACTOR_H__

#include "actor/fg_actor.h"

typedef enum { FG_LP8_MEASURE } fg_lp8_actor_message_code_t;

FG_ACTOR_INTERFACE_DEC(lp8, fg_lp8_actor_message_code_t);

typedef struct
{
    uint32_t conc;             // raw concentration value in ppm
    uint32_t conc_pc;          // pressure corrected concentration value in ppm
    uint32_t conc_filtered;    // filtered concentration value in ppm
    uint32_t conc_filtered_pc; // pressure corrected filtered concentration value in ppm
    int32_t temperature;       // internally measured temperature in units of 0.01 degrees Celsius
} fg_lp8_measurement_t;

#endif // FG_LP8_ACTOR_H__