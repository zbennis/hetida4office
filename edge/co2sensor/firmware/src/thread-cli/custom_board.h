#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#include "nrf_gpio.h"

// LEDs definitions for Adafruit ItsyBitsy nRF52840 Express
#define LEDS_NUMBER    4

#define LED_1          NRF_GPIO_PIN_MAP(0,6)
#define LED_2          NRF_GPIO_PIN_MAP(0,4)
#define LED_3          NRF_GPIO_PIN_MAP(0,30)
#define LED_4          NRF_GPIO_PIN_MAP(0,28)
#define LED_START      LED_1
#define LED_STOP       LED_4

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_1, LED_2 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      6
#define BSP_LED_1      4
#define BSP_LED_2      28

#define BUTTONS_NUMBER 1

#define BUTTON_1       29
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define RX_PIN_NUMBER  25
#define TX_PIN_NUMBER  24
#define HWFC           false

#endif // CUSTOM_BOARD_H
