#include "pins.h"
#include "fg_lpcomp_actor.h"
#include <app_util_platform.h>
#include <nrfx.h>
#include <nrfx_lpcomp.h>

/** Logging */
#define NRFX_LOG_MODULE FG_LPCOMP_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_lpcomp_start, fg_lpcomp_stop);

#define FG_LPCOMP_ACTOR_SLOT_ASSIGNMENT                                                            \
    {                                                                                              \
        [FG_LPCOMP_START] = fg_lpcomp_start, [FG_LPCOMP_STOP] = fg_lpcomp_stop                     \
    }
#define FG_LPCOMP_ACTOR_STATES                                                                     \
    {                                                                                              \
        LPCOMP_UNINITIALIZED, LPCOMP_OFF, LPCOMP_ACTIVE                                            \
    }
FG_ACTOR_DEF(FG_LPCOMP_ACTOR_SLOT_ASSIGNMENT, FG_LPCOMP_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_lpcomp_triggered_evt_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** LPCOMP resources */
// TODO: Pass in configuration from the outside comparable to GPIO actor.
#define AIN_VCAP_LPCOMP PIN_LP8_VCAP_LPCOMP
#define AIN_VCAP_VOLTAGE NRF_LPCOMP_REF_SUPPLY_15_16
#define AIN_VCAP_HYST NRF_LPCOMP_HYST_50mV

static void lpcomp_irq_handler(nrf_lpcomp_event_t event);


/** Public API */
FG_ACTOR_INIT_DEF(lpcomp, LPCOMP_UNINITIALIZED, LPCOMP_OFF, fg_lpcomp_init);


/** Internal implementation */
static void fg_lpcomp_init(void) {}

FG_ACTOR_SLOT(fg_lpcomp_start)
{
    FG_ACTOR_STATE_TRANSITION(LPCOMP_OFF, LPCOMP_ACTIVE, "activating");

    nrfx_lpcomp_config_t config = {.hal = {.reference = AIN_VCAP_VOLTAGE,
                                       .detection = NRF_LPCOMP_DETECT_UP,
                                       .hyst = AIN_VCAP_HYST},
        .input = AIN_VCAP_LPCOMP,
        .interrupt_priority = APP_IRQ_PRIORITY_LOWEST};
    DRVX(nrfx_lpcomp_init(&config, lpcomp_irq_handler));

    FG_ACTOR_RUN_SINGLETON_TASK(fg_lpcomp_triggered_evt_cb);

    nrf_lpcomp_shorts_enable(NRF_LPCOMP_SHORT_UP_STOP_MASK);
    nrf_lpcomp_int_enable(LPCOMP_INTENSET_READY_Msk);
    nrfx_lpcomp_enable();
}

void fg_lpcomp_shutdown(void);

FG_ACTOR_TASK_CALLBACK(fg_lpcomp_triggered_evt_cb) { fg_lpcomp_shutdown(); }

FG_ACTOR_SLOT(fg_lpcomp_stop) { fg_lpcomp_shutdown(); }

void fg_lpcomp_shutdown(void)
{
    if (m_fg_actor_state == LPCOMP_OFF)
        return;

    FG_ACTOR_STATE_TRANSITION(LPCOMP_ACTIVE, LPCOMP_OFF, "stopped");

    nrfx_lpcomp_uninit();
}

FG_ACTOR_INTERFACE_DEF(lpcomp, fg_lpcomp_actor_message_code_t)

// Obs: Will be called from interrupt context.
static void lpcomp_irq_handler(nrf_lpcomp_event_t event)
{
    if (m_fg_actor_state != LPCOMP_ACTIVE)
        return;

    uint32_t result;
    switch (event)
    {
        case NRF_LPCOMP_EVENT_READY:
            result = nrf_lpcomp_result_get();
            if (result == LPCOMP_RESULT_RESULT_Above)
                FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case NRF_LPCOMP_EVENT_UP:
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        default:
            // ignore other interrupts
            break;
    }
}