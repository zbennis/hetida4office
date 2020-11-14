#ifndef FG_UART_ACTOR_H
#define FG_UART_ACTOR_H

#include "actor/fg_actor.h"

typedef enum {
    FG_UART_ENABLE,
    FG_UART_DISABLE,
    FG_UART_TX,
    FG_UART_RX
} fg_uart_actor_message_code_t;

typedef struct
{
    uint8_t * p_data;
    size_t size;
} fg_uart_actor_rxtx_buffer_t;

FG_ACTOR_INTERFACE_DEC(uart, fg_uart_actor_message_code_t);

#endif // FG_UART_ACTOR_H__