#include "fg_rtc_actor.h"
#include <app_util_platform.h>
#include <nrfx_clock.h>
#include <nrfx_rtc.h>

/** Logging */
#define NRFX_FG_RTC_ACTOR_CONFIG_LOG_ENABLED 1
#define NRFX_FG_RTC_ACTOR_CONFIG_LOG_LEVEL 4
#define NRFX_FG_RTC_ACTOR_CONFIG_INFO_COLOR 3
#define NRFX_FG_RTC_ACTOR_CONFIG_DEBUG_COLOR 3
#define NRFX_LOG_MODULE FG_RTC_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_rtc_enable, fg_rtc_disable, fg_rtc_start_timer);

#define FG_RTC_ACTOR_SLOT_ASSIGNMENT                                                               \
    {                                                                                              \
        [FG_RTC_ENABLE] = fg_rtc_enable, [FG_RTC_DISABLE] = fg_rtc_disable,                        \
        [FG_RTC_START_TIMER] = fg_rtc_start_timer                                                  \
    }
#define FG_RTC_ACTOR_STATES                                                                        \
    {                                                                                              \
        RTC_UNINITIALIZED, RTC_OFF, RTC_LFCLK_REQUESTED, RTC_IDLE, RTC_RUNNING                     \
    }
FG_ACTOR_DEF(FG_RTC_ACTOR_SLOT_ASSIGNMENT, FG_RTC_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_rtc_lfclk_started_evt_cb, fg_rtc_compare_evt_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** Clock resources */
static void clock_irq_handler(nrfx_clock_evt_type_t event);


/** RTC resources */
#define FG_RTC_ACTOR_RTC_INSTANCE_ID 0
#define FG_RTC_ACTOR_CC_CHANNEL_ID 0

STATIC_ASSERT(RTC_INPUT_FREQ % FG_RTC_TICKS_PER_SECOND == 0,
    "RTC ticks per second must be a divisor of 32768 Hz");

#define FG_RTC_ACTOR_MIN_TICKS 2 // see nRF52840 spec, 6.22.7
#define FG_RTC_ACTOR_PRESCALER                                                                     \
    ((RTC_INPUT_FREQ / FG_RTC_TICKS_PER_SECOND) - 1) // see nRF52840 spec, 6.22.2
STATIC_ASSERT(FG_RTC_ACTOR_PRESCALER <= RTC_PRESCALER_PRESCALER_Msk);

static const nrfx_rtc_t m_nrfx_rtc = NRFX_RTC_INSTANCE(FG_RTC_ACTOR_RTC_INSTANCE_ID);
static void rtc_irq_handler(nrfx_rtc_int_type_t event);

uint32_t m_rtc_enable_calls = 0;


/** Public API */
FG_ACTOR_INIT_DEF(rtc, RTC_UNINITIALIZED, RTC_OFF, fg_rtc_init);


/** Internal implementation */
static void fg_rtc_init(void) { APP_ERROR_CHECK(nrfx_clock_init(clock_irq_handler)); }

FG_ACTOR_SLOT(fg_rtc_enable)
{
    m_rtc_enable_calls++;
    if (m_fg_actor_state > RTC_OFF)
        return;

    FG_ACTOR_STATE_TRANSITION(RTC_OFF, RTC_LFCLK_REQUESTED, "requesting LFCLK");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_rtc_lfclk_started_evt_cb);

    // Obs: Requesting the LFCLK has a measured overhead of about 1000us
    // if it is not yet running (see PS 5.4.4.4, tSTART_LFRC). Using the
    // nrfx_clock API guarantees interoperability of the LFCLK with
    // other users (notably the Thread stack).
    nrfx_clock_enable();
    nrfx_clock_lfclk_start();
}

FG_ACTOR_TASK_CALLBACK(fg_rtc_lfclk_started_evt_cb)
{
    FG_ACTOR_STATE_TRANSITION(RTC_LFCLK_REQUESTED, RTC_IDLE, "LFCLK started");
}

FG_ACTOR_SLOT(fg_rtc_disable)
{
    m_rtc_enable_calls--;
    if (m_fg_actor_state == RTC_OFF || m_rtc_enable_calls > 0)
        return;

    // The following implicit state transition
    // check ensures that we never release
    // the clock more than once as this
    // would make it stop for other software
    // that requested it!
    FG_ACTOR_STATE_TRANSITION(RTC_IDLE, RTC_OFF, "releasing LFCLK");

    nrfx_clock_lfclk_stop();
    nrfx_clock_disable();
}

FG_ACTOR_SLOT(fg_rtc_start_timer)
{
    FG_ACTOR_STATE_TRANSITION(RTC_IDLE, RTC_RUNNING, "starting RTC timer");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_rtc_compare_evt_cb);

    // Initialize RTC instance.
    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler = FG_RTC_ACTOR_PRESCALER;
    DRVX(nrfx_rtc_init(&m_nrfx_rtc, &config, rtc_irq_handler));

    // Get the timeout from the message.
    FG_ACTOR_GET_ARGS(
        uint32_t, timeout, p_calling_action); // in multiples of the prescaled frequency.

    // Make sure the timeout is within reachable limits.
    ASSERT(timeout >= FG_RTC_ACTOR_MIN_TICKS)
    ASSERT(timeout <= RTC_COUNTER_COUNTER_Msk)

    // Set compare channel to trigger interrupt and enable event
    DRVX(nrfx_rtc_cc_set(&m_nrfx_rtc, FG_RTC_ACTOR_CC_CHANNEL_ID, timeout, true));

    ASSERT(nrfx_rtc_counter_get(&m_nrfx_rtc) == 0)

    nrfx_rtc_enable(&m_nrfx_rtc);
}

FG_ACTOR_TASK_CALLBACK(fg_rtc_compare_evt_cb)
{
    FG_ACTOR_STATE_TRANSITION(RTC_RUNNING, RTC_IDLE, "RTC timer fired");

    nrfx_rtc_uninit(&m_nrfx_rtc);
    nrfx_rtc_counter_clear(&m_nrfx_rtc);
}

FG_ACTOR_INTERFACE_DEF(rtc, fg_rtc_actor_message_code_t)

static void clock_irq_handler(nrfx_clock_evt_type_t event)
{
    if (m_fg_actor_state != RTC_LFCLK_REQUESTED)
        return;

    switch (event)
    {
        case NRFX_CLOCK_EVT_LFCLK_STARTED:
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        default:
            // ignore other events
            break;
    }
}

static void rtc_irq_handler(nrfx_rtc_int_type_t event)
{
    if (m_fg_actor_state != RTC_RUNNING)
        return;

    switch (event)
    {
        case NRFX_RTC_INT_COMPARE0:
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        default:
            // ignore other events
            break;
    }
}