#ifndef FG_BME280_ACTOR_H__
#define FG_BME280_ACTOR_H__

#include "actor/fg_actor.h"
#include "lib/bme280.h"

typedef enum { FG_BME280_RESET, FG_BME280_MEASURE } fg_bme280_actor_message_code_t;

FG_ACTOR_INTERFACE_DEC(bme280, fg_bme280_actor_message_code_t);

typedef struct bme280_data fg_bme280_measurement_t;

#endif // FG_BME280_ACTOR_H__