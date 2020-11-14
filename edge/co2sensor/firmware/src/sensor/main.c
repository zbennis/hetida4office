#include "bme280/fg_bme280_actor.h"
#include "lp8/fg_lp8_actor.h"
#include "rtc/fg_rtc_actor.h"
#include <nrf_assert.h>
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#include <nrf_pwr_mgmt.h>
#include <stdlib.h>
  
// Time between measurements
#define MAIN_IDLE_TIME                                                                             \
    (FG_RTC_TICKS_PER_SECOND * 16) // min 16s (min time between LP8 measurements), currently 16s

FG_ACTOR_RESULT_HANDLERS_DEC(start_timer_result_handler, init_or_wakeup_result_handler,
    pressure_measurement_result_handler, co2_measurement_result_handler);

/** BME280 child actor resources */
static const fg_actor_t * m_p_bme280_actor;

/** LP8 child actor resources */
static const fg_actor_t * m_p_lp8_actor;

/** RTC child actor resources */
static const fg_actor_t * m_p_rtc_actor;

/** Main loop */

// TODO: upgrade nRF5 SDK for Thread and Zigbee version
// TODO: re-measure RTC actor to make sure it still works as intended (timings + current
//       consumption)
// TODO: implement board.h to support thread connection indicator LEDs
// TODO: combine with Thread and MQTT-SN protocols
// TODO: install and use DC/DC regulator
// TODO: Make sure EN_BLK_REVERSE is Hi-Z during reset to make sure that VCAP will not
//       (reverse) aliment 3V3.
// TODO: implement a keep-alive LED that shortly flashes whenever a keep-alive packet is sent out
// TODO: implement low-power detection together with a low-power buzzer
// TODO: use the buzzer also to indicate dangerously high levels of CO2 independently
// TODO: implement watchdog (capture ASSERTS and show it somehow e.g. via buzzer), stack guard, mpu,
//       NRFX_PRS
// TODO: deep sleep (System OFF) between measurements
// TODO: save sensor state in non-volatile memory after re-calibration and
//       re-load it after reset (verify first if calibration data is not saved on
//       chip anyway).
int main(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("FG sensor app started.");

    APP_ERROR_CHECK(nrf_pwr_mgmt_init());

    fg_actor_init();

    m_p_bme280_actor = fg_bme280_actor_init();
    m_p_lp8_actor = fg_lp8_actor_init();
    m_p_rtc_actor = fg_rtc_actor_init();

    // TODO: Implement RTC Actor in a way that it can handle multiple
    // concurrent ENABLE messages, then start the timer in parallel with
    // the BME reset.
    fg_actor_transaction_t * const p_init_transaction =
        fg_actor_allocate_root_transaction(start_timer_result_handler);
    fg_rtc_actor_allocate_message(FG_RTC_ENABLE, p_init_transaction);

    while (true)
    {
        fg_actor_run();
        NRF_LOG_FLUSH();
        nrf_pwr_mgmt_run();
    }
}

/** Implementation */
FG_ACTOR_RESULT_HANDLER(start_timer_result_handler)
{
    const fg_actor_action_t * const p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    ASSERT(p_completed_action->p_actor == m_p_rtc_actor)
    ASSERT(p_completed_action->message.code == FG_RTC_ENABLE)
    CHECK_COMPLETED_ACTION(p_completed_action, "RTC enable error!");

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(bme280, FG_BME280_RESET);
    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(init_or_wakeup_result_handler);
}

FG_ACTOR_RESULT_HANDLER(init_or_wakeup_result_handler)
{
    static fg_bme280_measurement_t bme280_measurement;

    const fg_actor_action_t * p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    if (p_completed_action->p_actor == m_p_bme280_actor)
    {
        ASSERT(p_completed_action->message.code == FG_BME280_RESET)
        CHECK_COMPLETED_ACTION(p_completed_action, "BME280 initialization error!");
    }
    else if (p_completed_action->p_actor == m_p_rtc_actor)
    {
        ASSERT(p_completed_action->message.code == FG_RTC_START_TIMER);
        CHECK_COMPLETED_ACTION(p_completed_action, "RTC error!");
    }
    else
    {
        // Unexpected actor.
        ASSERT(false)
    }

    // Start measuring.
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(bme280, FG_BME280_MEASURE);
    FG_ACTOR_SET_P_RESULT(p_next_action, fg_bme280_measurement_t, &bme280_measurement);
    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(pressure_measurement_result_handler);
}

FG_ACTOR_RESULT_HANDLER(pressure_measurement_result_handler)
{
    static uint16_t pressure;
    static fg_lp8_measurement_t lp8_measurement;

    const fg_actor_action_t * const p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    CHECK_COMPLETED_ACTION(p_completed_action, "BME280 measurement error!");

    ASSERT(p_completed_action->p_actor == m_p_bme280_actor)
    ASSERT(p_completed_action->message.code == FG_BME280_MEASURE)

    FG_ACTOR_GET_P_RESULT(fg_bme280_measurement_t, p_measurement_result, p_completed_action);
    NRF_LOG_INFO("Measurement: pressure %u, temperature %d, humidity %u\n",
        p_measurement_result->pressure, p_measurement_result->temperature,
        p_measurement_result->humidity);

    // BME280 returns pressure in units of 1/100Pa, LP8 expects units of 10Pa = 0.1hPa
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(lp8, FG_LP8_MEASURE);
    pressure = p_measurement_result->pressure / 1000;
    FG_ACTOR_SET_ARGS(p_next_action, pressure);

    FG_ACTOR_SET_P_RESULT(p_next_action, fg_lp8_measurement_t, &lp8_measurement);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(co2_measurement_result_handler);
}

FG_ACTOR_RESULT_HANDLER(co2_measurement_result_handler)
{
    const fg_actor_action_t * const p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    CHECK_COMPLETED_ACTION(p_completed_action, "LP8 measurement error!");

    ASSERT(p_completed_action->p_actor == m_p_lp8_actor)
    ASSERT(p_completed_action->message.code == FG_LP8_MEASURE)

    FG_ACTOR_GET_P_RESULT(fg_lp8_measurement_t, p_measurement_result, p_completed_action);
    NRF_LOG_INFO("Measurement: filtered %u (raw %u) PPM CO2, %d C.\n",
        p_measurement_result->conc_filtered_pc, p_measurement_result->conc_pc,
        p_measurement_result->temperature);

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_START_TIMER);
    static const uint32_t m_main_idle_time = MAIN_IDLE_TIME;
    FG_ACTOR_SET_ARGS(p_next_action, m_main_idle_time);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(init_or_wakeup_result_handler);
}