#ifndef FG_GPIO_ACTOR_H
#define FG_GPIO_ACTOR_H

#include "actor/fg_actor.h"
#include <nrfx_gpiote.h>

typedef enum { FG_GPIO_LISTEN_UP, FG_GPIO_LISTEN_DOWN, FG_GPIO_STOP } fg_gpio_actor_message_code_t;

typedef struct
{
    nrfx_gpiote_pin_t pin;
    nrf_gpio_pin_pull_t pull;
} fg_gpio_pin_config_t;

FG_ACTOR_INTERFACE_DEC(gpio, fg_gpio_actor_message_code_t);

#endif // FG_GPIO_ACTOR_H