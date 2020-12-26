#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#include "nrf_gpio.h"

// LEDs definitions for Adafruit Feather nRF52840 Express
#define LEDS_NUMBER    2

#define LED_1          NRF_GPIO_PIN_MAP(1,15)
#define LED_2          NRF_GPIO_PIN_MAP(1,10)
#define LED_START      LED_1
#define LED_STOP       LED_2

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      47
#define BSP_LED_1      42

#define BUTTONS_NUMBER 1

#define BUTTON_1       34
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define RX_PIN_NUMBER  24
#define TX_PIN_NUMBER  25
#define HWFC           false

#endif // CUSTOM_BOARD_H
