#include "saadc/fg_saadc_actor.h"
#include "gpio/fg_gpio.h"
#include "pins.h"
#include <nrfx.h>
#include <nrfx_saadc.h>

/** Logging */
#define NRFX_LOG_MODULE FG_SAADC_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_saadc_measure, fg_saadc_calibrate);

#define FG_SAADC_ACTOR_SLOT_ASSIGNMENT                                                             \
    {                                                                                              \
        [FG_SAADC_MEASURE] = fg_saadc_measure, [FG_SAADC_CALIBRATE] = fg_saadc_calibrate           \
    }
#define FG_SAADC_ACTOR_STATES                                                                      \
    {                                                                                              \
        SAADC_UNINITIALIZED, SAADC_OFF, SAADC_MEASURING, SAADC_CALIBRATING                         \
    }
FG_ACTOR_DEF(FG_SAADC_ACTOR_SLOT_ASSIGNMENT, FG_SAADC_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_saadc_measure_cb, fg_saadc_calibrate_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** SAADC resources */
// TODO: Pass in configuration from the outside comparable to GPIO actor.
#define FG_SAADC_PIN_nEN PIN_BAT_nEN_MEAS
#define FG_SAADC_PIN PIN_BAT_MEAS_SAADC

#define FG_SAADC_CHANNEL 0

#define FG_SAADC_SAMPLE_AQC_TIME_CONFIG NRF_SAADC_ACQTIME_20US
#define FG_SAADC_SAMPLE_AQC_TIME                                                                   \
    20 // Sample acquisition time for up to 400k external resistance [µs], see nRF52840 spec,
       // ch. 6.23.1.1
#define FG_SAADC_SAMPLE_CONV_TIME                                                                  \
    2 // Sample conversion time [µs] - see tCONV, nRF52840 spec, ch. 6.23.10.1
#define FG_SAADC_SAMPLE_MARGIN 13 // Time between samples [µs]
#define FG_SAADC_SAMPLE_FREQUENCY                                                                  \
    (1000000U / (FG_SAADC_SAMPLE_AQC_TIME + FG_SAADC_SAMPLE_CONV_TIME +                             \
                   FG_SAADC_SAMPLE_MARGIN)) // [Hz], see nRF52840 spec, ch. 6.23.5
#define FG_SAADC_SAMPLE_CLOCK_FREQUENCY 16000000U // [Hz], see nRF52840 spec, ch. 6.23.9.12
#define FG_SAADC_SAMPLE_RATE CEIL_DIV(FG_SAADC_SAMPLE_CLOCK_FREQUENCY, FG_SAADC_SAMPLE_FREQUENCY)
STATIC_ASSERT(FG_SAADC_SAMPLE_RATE >= 80);
STATIC_ASSERT(FG_SAADC_SAMPLE_RATE <= 2047);

#define FG_SAADC_CONVERSION_BASE_REFERENCE 600000U // ADC internal reference voltage [µV]
#define FG_SAADC_CONVERSION_REVERSE_GAIN 6U // ADC reverse gain factor, i.e. 1 / NRF_SAADC_GAIN1_6
#define FG_SAADC_CONVERSION_REFERENCE                                                              \
    (FG_SAADC_CONVERSION_BASE_REFERENCE * FG_SAADC_CONVERSION_REVERSE_GAIN)
#define FG_SAADC_CONVERSION_MAX_VALUE ((1U << (8 + 2 * NRFX_SAADC_CONFIG_RESOLUTION)) - 1)
#define FG_SAADC_CONVERSION_FACTOR                                                                 \
    ROUNDED_DIV(FG_SAADC_CONVERSION_REFERENCE, FG_SAADC_CONVERSION_MAX_VALUE)

static void saadc_irq_handler(nrfx_saadc_evt_t const * p_event);

static nrf_saadc_value_t m_fg_saadc_buffer;


/** Public API */
FG_ACTOR_INIT_DEF(saadc, SAADC_UNINITIALIZED, SAADC_OFF, fg_saadc_init);


/** Internal implementation */
static void fg_saadc_init(void)
{
    fg_gpio_cfg_out_od_nopull(FG_SAADC_PIN_nEN);
    nrf_saadc_continuous_mode_enable(FG_SAADC_SAMPLE_RATE);
}

static void fg_saadc_enable(fg_actor_action_t * p_calling_action)
{
    const nrfx_saadc_config_t config = NRFX_SAADC_DEFAULT_CONFIG;
    DRVX(nrfx_saadc_init(&config, saadc_irq_handler));
}

static void fg_saadc_disable(void) { nrfx_saadc_uninit(); }

FG_ACTOR_SLOT(fg_saadc_measure)
{
    FG_ACTOR_STATE_TRANSITION(SAADC_OFF, SAADC_MEASURING, "starting SAADC measurement");
    FG_ACTOR_RUN_SINGLETON_TASK(fg_saadc_measure_cb);

    fg_saadc_enable(p_calling_action);

    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(FG_SAADC_PIN);
    channel_config.acq_time = FG_SAADC_SAMPLE_AQC_TIME_CONFIG;
    DRVX(nrfx_saadc_channel_init(FG_SAADC_CHANNEL, &channel_config));

    nrf_gpio_pin_clear(FG_SAADC_PIN_nEN);

    ASSERT(!nrfx_saadc_is_busy())
    DRVX(nrfx_saadc_buffer_convert(&m_fg_saadc_buffer, 1));
    DRVX(nrfx_saadc_sample());
}

FG_ACTOR_TASK_CALLBACK(fg_saadc_measure_cb)
{
    FG_ACTOR_STATE_TRANSITION(SAADC_MEASURING, SAADC_OFF, "SAADC measurement finished");

    nrf_gpio_pin_set(FG_SAADC_PIN_nEN);
    fg_saadc_disable();

    ASSERT(p_completed_action->result_size == sizeof(nrf_saadc_value_t));
    ASSERT(p_completed_action->p_result);
    ASSERT(p_calling_action->result_size == sizeof(fg_saadc_result_t))
    ASSERT(p_calling_action->p_result)

    const nrf_saadc_value_t const * p_measurement = p_completed_action->p_result;
    fg_saadc_result_t * const p_result = p_calling_action->p_result;
    *p_result = (fg_saadc_result_t)(*p_measurement * FG_SAADC_CONVERSION_FACTOR);
}

FG_ACTOR_SLOT(fg_saadc_calibrate)
{
    FG_ACTOR_STATE_TRANSITION(SAADC_OFF, SAADC_CALIBRATING, "starting SAADC calibration");
    FG_ACTOR_RUN_SINGLETON_TASK(fg_saadc_calibrate_cb);

    fg_saadc_enable(p_calling_action);

    ASSERT(!nrfx_saadc_is_busy())
    DRVX(nrfx_saadc_calibrate_offset());
}

FG_ACTOR_TASK_CALLBACK(fg_saadc_calibrate_cb)
{
    FG_ACTOR_STATE_TRANSITION(SAADC_CALIBRATING, SAADC_OFF, "SAADC calibration finished");

    fg_saadc_disable();
}

FG_ACTOR_INTERFACE_DEF(saadc, fg_saadc_actor_message_code_t)

// Obs: Will be called from interrupt context.
static void saadc_irq_handler(nrfx_saadc_evt_t const * p_event)
{
    if (!(m_fg_actor_state == SAADC_MEASURING || m_fg_actor_state == SAADC_CALIBRATING))
        return;

    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_DONE:
        {
            ASSERT(p_event->data.done.size == 1)

            fg_actor_action_t * const p_running_task = FG_ACTOR_GET_SINGLETON_TASK();
            p_running_task->p_result = p_event->data.done.p_buffer;
            p_running_task->result_size = sizeof(nrf_saadc_value_t);

            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;
        }

        case NRFX_SAADC_EVT_CALIBRATEDONE:
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        default:
            // ignore other interrupts
            break;
    }
}