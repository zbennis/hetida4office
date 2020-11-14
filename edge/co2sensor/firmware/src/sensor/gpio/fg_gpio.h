#ifndef FG_GPIO_H
#define FG_GPIO_H

#include "hal/nrf_gpio.h"

__STATIC_INLINE void fg_gpio_cfg_in_od(uint32_t pin_number)
{
    nrf_gpio_cfg_input(pin_number, NRF_GPIO_PIN_PULLUP);
}

__STATIC_INLINE void fg_gpio_cfg_out_os(uint32_t pin_number)
{
    nrf_gpio_pin_clear(pin_number);
    nrf_gpio_cfg(pin_number, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_D0S1, NRF_GPIO_PIN_NOSENSE);
}

__STATIC_INLINE void fg_gpio_cfg_out_os_nopull(uint32_t pin_number)
{
    nrf_gpio_pin_clear(pin_number);
    nrf_gpio_cfg(pin_number, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_D0S1, NRF_GPIO_PIN_NOSENSE);
}

__STATIC_INLINE void fg_gpio_cfg_out_od_bidir(uint32_t pin_number)
{
    nrf_gpio_pin_set(pin_number);
    nrf_gpio_cfg(pin_number, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_CONNECT,
        NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_S0D1, NRF_GPIO_PIN_NOSENSE);
}

__STATIC_INLINE void fg_gpio_cfg_out_od_nopull(uint32_t pin_number)
{
    nrf_gpio_pin_set(pin_number);
    nrf_gpio_cfg(pin_number, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0D1, NRF_GPIO_PIN_NOSENSE);
}

__STATIC_INLINE void fg_gpio_cfg_out_pp(uint32_t pin_number)
{
    nrf_gpio_pin_clear(pin_number);
    nrf_gpio_cfg(pin_number, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
}

__STATIC_INLINE nrf_gpio_pin_pull_t fg_gpio_pin_pull_get(uint32_t pin_number)
{
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);

    return (nrf_gpio_pin_pull_t)((reg->PIN_CNF[pin_number] &
                                   GPIO_PIN_CNF_PULL_Msk) >> GPIO_PIN_CNF_PULL_Pos);
}


#endif // FG_GPIO_H