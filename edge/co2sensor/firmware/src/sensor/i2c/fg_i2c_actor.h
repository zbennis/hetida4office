#ifndef FG_I2C_ACTOR_H
#define FG_I2C_ACTOR_H

#include "actor/fg_actor.h"

typedef enum {
    FG_I2C_ENABLE,
    FG_I2C_DISABLE,
    FG_I2C_TRANSACT
} fg_i2c_actor_message_code_t;

typedef struct {
    uint8_t address;
    uint8_t * p_tx_data;
    size_t tx_size;
    uint8_t * p_rx_data;
    size_t rx_size;
} fg_i2c_actor_transaction_t;
STATIC_ASSERT(sizeof(fg_i2c_actor_transaction_t) == 5 * sizeof(uint32_t));

FG_ACTOR_INTERFACE_DEC(i2c, fg_i2c_actor_message_code_t);

#endif // FG_I2C_ACTOR_H__
