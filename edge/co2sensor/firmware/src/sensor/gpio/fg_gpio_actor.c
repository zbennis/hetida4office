#include "fg_gpio_actor.h"
#include "fg_gpio.h"
#include "pins.h"
#include <app_util_platform.h>

/** Logging */
#define NRFX_LOG_MODULE FG_GPIO_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_gpio_listen_up, fg_gpio_listen_down, fg_gpio_stop);

#define FG_GPIO_ACTOR_SLOT_ASSIGNMENT                                                              \
    {                                                                                              \
        [FG_GPIO_LISTEN_UP] = fg_gpio_listen_up, [FG_GPIO_LISTEN_DOWN] = fg_gpio_listen_down,      \
        [FG_GPIO_STOP] = fg_gpio_stop                                                              \
    }
#define FG_GPIO_ACTOR_STATES                                                                       \
    {                                                                                              \
        GPIO_UNINITIALIZED, GPIO_OFF, GPIO_LISTENING                                               \
    }
#define FG_GPIO_ACTOR_MAX_CONCURRENT_TASKS NUM_DIGITAL_IN
FG_ACTOR_DEF(
    FG_GPIO_ACTOR_SLOT_ASSIGNMENT, FG_GPIO_ACTOR_STATES, FG_GPIO_ACTOR_MAX_CONCURRENT_TASKS);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_gpio_triggered_evt_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** GPIO resources */
static uint8_t m_num_listening = 0; // the number of currently active listeners
static uint32_t m_pins_by_task[FG_GPIO_ACTOR_MAX_CONCURRENT_TASKS];

static void gpio_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);


/** Public API */
FG_ACTOR_INIT_DEF(gpio, GPIO_UNINITIALIZED, GPIO_OFF, fg_gpio_init);


/** Internal implementation */
#define TASK_NOT_FOUND 0xff

static uint8_t fg_gpio_get_task_id_by_pin(uint32_t pin)
{
    uint8_t task_id = 0;
    while (task_id < m_num_listening)
    {
        if (m_pins_by_task[task_id] == pin)
            break;

        task_id++;
    }

    if (task_id == m_num_listening)
        return TASK_NOT_FOUND;

    return task_id;
}

static void fg_gpio_init(void) {}

#define FG_GPIO_UP true
#define FG_GPIO_DOWN false
static void fg_gpio_listen(FG_ACTOR_MESSAGE_HANDLER_ARGS_DEC, bool up);

FG_ACTOR_SLOT(fg_gpio_listen_up) { fg_gpio_listen(FG_ACTOR_MESSAGE_HANDLER_ARGS, FG_GPIO_UP); }

FG_ACTOR_SLOT(fg_gpio_listen_down) { fg_gpio_listen(FG_ACTOR_MESSAGE_HANDLER_ARGS, FG_GPIO_DOWN); }

static void fg_gpio_listen(FG_ACTOR_MESSAGE_HANDLER_ARGS_DEC, bool up)
{
    if (m_fg_actor_state == GPIO_OFF)
    {
        ASSERT(m_num_listening == 0);
        FG_ACTOR_STATE_TRANSITION(GPIO_OFF, GPIO_LISTENING, "enabling and adding listener");
        DRVX(nrfx_gpiote_init());
    }
    else
    {
        FG_ACTOR_STATE_TRANSITION(GPIO_LISTENING, GPIO_LISTENING, "adding listener");
    }

    ASSERT(m_num_listening < FG_GPIO_ACTOR_MAX_CONCURRENT_TASKS);

    FG_ACTOR_GET_P_ARGS(fg_gpio_pin_config_t, p_pin_config, p_calling_action);

    // Check that only a single listener task can be attached per pin.
    for (uint8_t task_id = 0; task_id < m_num_listening; task_id++)
    {
        ASSERT(m_pins_by_task[task_id] != p_pin_config->pin);
    }

    uint8_t task_id = m_num_listening;
    m_num_listening++;
    m_pins_by_task[task_id] = p_pin_config->pin;

    nrfx_gpiote_in_config_t config = {.is_watcher = false,
        .hi_accuracy = false, // low power, low accuracy
        .pull = p_pin_config->pull,
        .sense = up ? NRF_GPIOTE_POLARITY_LOTOHI : NRF_GPIOTE_POLARITY_HITOLO,
        .skip_gpio_setup = false};
    DRVX(nrfx_gpiote_in_init(p_pin_config->pin, &config, gpio_irq_handler));

    FG_ACTOR_RUN_TASK(task_id, fg_gpio_triggered_evt_cb);

    nrfx_gpiote_in_event_enable(p_pin_config->pin, true);
}

static void fg_gpio_shutdown(const fg_actor_action_t * const p_calling_action);

FG_ACTOR_TASK_CALLBACK(fg_gpio_triggered_evt_cb) { fg_gpio_shutdown(p_calling_action); }

FG_ACTOR_SLOT(fg_gpio_stop) { fg_gpio_shutdown(p_calling_action); }

static void fg_gpio_shutdown(const fg_actor_action_t * const p_calling_action)
{
    if (m_fg_actor_state == GPIO_OFF)
        return;

    FG_ACTOR_GET_P_ARGS(fg_gpio_pin_config_t, p_pin_config, p_calling_action);

    nrfx_gpiote_in_uninit(p_pin_config->pin);

    uint8_t task_id = fg_gpio_get_task_id_by_pin(p_pin_config->pin);

    ASSERT(m_num_listening > 0 && task_id != TASK_NOT_FOUND)

    m_num_listening--;
    m_pins_by_task[task_id] = 0;

    if (m_num_listening == 0)
    {
        FG_ACTOR_STATE_TRANSITION(GPIO_LISTENING, GPIO_OFF, "removed listener and disabled");
        nrfx_gpiote_uninit();
    }
    else
    {
        FG_ACTOR_STATE_TRANSITION(GPIO_LISTENING, GPIO_LISTENING, "removed listener");
    }
}

FG_ACTOR_INTERFACE_DEF(gpio, fg_gpio_actor_message_code_t)

// Obs: Will be called from interrupt context.
static void gpio_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t polarity)
{
    // Ignore triggers that come in after stopping the actor.
    if (m_fg_actor_state != GPIO_LISTENING)
        return;

    uint8_t task_id;
    switch (polarity)
    {
        case NRF_GPIOTE_POLARITY_LOTOHI:
        case NRF_GPIOTE_POLARITY_HITOLO:
            task_id = fg_gpio_get_task_id_by_pin(pin);
            if (task_id == TASK_NOT_FOUND)
                return;
            fg_actor_task_finished(task_id);
            break;

        default:
            // ignore other interrupts
            break;
    }
}