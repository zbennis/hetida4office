#include "uart/fg_uart_actor.h"
#include "pins.h"
#include <nrfx.h>
#include <nrfx_uarte.h>

/** Logging */
#define NRFX_LOG_MODULE FG_UART_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_uart_enable, fg_uart_disable, fg_uart_tx, fg_uart_rx);

#define FG_UART_ACTOR_SLOT_ASSIGNMENT                                                              \
    {                                                                                              \
        [FG_UART_ENABLE] = fg_uart_enable, [FG_UART_DISABLE] = fg_uart_disable,                    \
        [FG_UART_TX] = fg_uart_tx, [FG_UART_RX] = fg_uart_rx                                       \
    }
#define FG_UART_ACTOR_STATES                                                                       \
    {                                                                                              \
        UART_UNINITIALIZED, UART_OFF, UART_ON                                                      \
    }
enum
{
    FG_UART_TASK_INSTANCE_TX,
    FG_UART_TASK_INSTANCE_RX,
    FG_UART_MAX_CONCURRENT_TASKS
};
FG_ACTOR_DEF(FG_UART_ACTOR_SLOT_ASSIGNMENT, FG_UART_ACTOR_STATES, FG_UART_MAX_CONCURRENT_TASKS);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_uart_tx_done_evt_cb, fg_uart_rx_done_evt_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** UARTE resources */
#define UARTE_INSTANCE_ID 1

static nrfx_uarte_t m_nrfx_uarte = NRFX_UARTE_INSTANCE(UARTE_INSTANCE_ID);
static void uarte_irq_handler(nrfx_uarte_event_t const * p_event, void * p_context);

uint32_t m_uart_enable_calls;


/** Public API */
FG_ACTOR_INIT_DEF(uart, UART_UNINITIALIZED, UART_OFF, fg_uart_init);


/** Internal implementation */
static void fg_uart_init(void)
{
    nrf_gpio_cfg_default(PIN_UART_RX);
    nrf_gpio_cfg_default(PIN_UART_TX);
}

FG_ACTOR_SLOT(fg_uart_enable)
{
    m_uart_enable_calls++;

    if (m_fg_actor_state > UART_OFF)
        return;

    FG_ACTOR_STATE_TRANSITION(UART_OFF, UART_ON, "enabling UART peripheral");

    nrfx_uarte_config_t config = NRFX_UARTE_DEFAULT_CONFIG;
    config.pseltxd = PIN_UART_TX;
    config.pselrxd = PIN_UART_RX;
    config.baudrate = NRF_UARTE_BAUDRATE_9600;
    config.stop = NRF_UARTE_STOP_TWO;

    DRVX(nrfx_uarte_init(&m_nrfx_uarte, &config, uarte_irq_handler));
}

FG_ACTOR_SLOT(fg_uart_disable)
{
    if (m_fg_actor_state == UART_OFF || --m_uart_enable_calls > 0)
        return;

    FG_ACTOR_STATE_TRANSITION(UART_ON, UART_OFF, "disabling UART peripheral");

    // TODO: Check whether some rx/tx task is still running and if so: stop it first (+ wait for the
    // stop event, see spec 6.34.7).
    nrfx_uarte_uninit(&m_nrfx_uarte);
}

FG_ACTOR_SLOT(fg_uart_tx)
{
    FG_ACTOR_STATE_TRANSITION(UART_ON, UART_ON, "UART tx");

    FG_ACTOR_RUN_TASK(FG_UART_TASK_INSTANCE_TX, fg_uart_tx_done_evt_cb);

    FG_ACTOR_GET_P_ARGS(fg_uart_actor_rxtx_buffer_t, p_uart_tx_buffer, p_calling_action);
    DRVX(nrfx_uarte_tx(&m_nrfx_uarte, p_uart_tx_buffer->p_data, p_uart_tx_buffer->size));
}

FG_ACTOR_TASK_CALLBACK(fg_uart_tx_done_evt_cb)
{
    FG_ACTOR_STATE_TRANSITION(UART_ON, UART_ON, "UART tx finished");
}

FG_ACTOR_SLOT(fg_uart_rx)
{
    FG_ACTOR_STATE_TRANSITION(UART_ON, UART_ON, "UART rx");

    FG_ACTOR_RUN_TASK(FG_UART_TASK_INSTANCE_RX, fg_uart_rx_done_evt_cb);

    FG_ACTOR_GET_P_ARGS(fg_uart_actor_rxtx_buffer_t, p_uart_rx_buffer, p_calling_action);
    DRVX(nrfx_uarte_rx(&m_nrfx_uarte, p_uart_rx_buffer->p_data, p_uart_rx_buffer->size));
}

FG_ACTOR_TASK_CALLBACK(fg_uart_rx_done_evt_cb)
{
    FG_ACTOR_STATE_TRANSITION(UART_ON, UART_ON, "UART rx finished");

    ASSERT(p_calling_action->result_size == p_completed_action->result_size);
    ASSERT(p_calling_action->p_result == p_completed_action->p_result);
}

FG_ACTOR_INTERFACE_DEF(uart, fg_uart_actor_message_code_t)

static bool matches_running_task(const fg_actor_action_t * const p_running_task,
    const nrfx_uarte_xfer_evt_t * const p_rx_buffer);

// Obs: Will be called from interrupt context.
static void uarte_irq_handler(nrfx_uarte_event_t const * p_event, void * p_context)
{
    if (m_fg_actor_state != UART_ON)
        return;

    fg_actor_action_t * p_running_task;

    const bool tx_running = fg_actor_is_task_running(FG_UART_TASK_INSTANCE_TX);
    const bool rx_running = fg_actor_is_task_running(FG_UART_TASK_INSTANCE_RX);

    switch (p_event->type)
    {
        case NRFX_UARTE_EVT_TX_DONE:
            if (!tx_running)
                break;

            fg_actor_task_finished(FG_UART_TASK_INSTANCE_TX);
            break;

        case NRFX_UARTE_EVT_RX_DONE:
            if (!rx_running)
                break;

            const nrfx_uarte_xfer_evt_t * const p_rx_buffer = &p_event->data.rxtx;
            p_running_task = fg_actor_get_running_task(FG_UART_TASK_INSTANCE_RX);
            if (!matches_running_task(p_running_task, p_rx_buffer))
            {
                FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_INVALID_LENGTH);
            }

            p_running_task->p_result = p_rx_buffer->p_data;
            p_running_task->result_size = p_rx_buffer->bytes;
            fg_actor_task_finished(FG_UART_TASK_INSTANCE_RX);
            break;

        case NRFX_UARTE_EVT_ERROR:
            // ignore break errors
            if (p_event->data.error.error_mask | NRF_UARTE_ERROR_BREAK_MASK)
                break;

            if (tx_running)
            {
                p_running_task = fg_actor_get_running_task(FG_UART_TASK_INSTANCE_TX);
                FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_INVALID_LENGTH);
                fg_actor_task_finished(FG_UART_TASK_INSTANCE_TX);
            }
            if (rx_running)
            {
                p_running_task = fg_actor_get_running_task(FG_UART_TASK_INSTANCE_RX);
                FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_INVALID_LENGTH);
                fg_actor_task_finished(FG_UART_TASK_INSTANCE_RX);
            }
            break;

        default:
            // ignore other interrupts
            break;
    }
}

static bool matches_running_task(
    const fg_actor_action_t * const p_running_task, const nrfx_uarte_xfer_evt_t * const p_rx_buffer)
{
    ASSERT(p_running_task)
    const fg_actor_transaction_t * const p_transaction = p_running_task->p_transaction;
    ASSERT(p_transaction)
    const fg_actor_action_t * const p_calling_action = p_transaction->p_calling_action;
    ASSERT(p_calling_action)

    FG_ACTOR_GET_P_ARGS(fg_uart_actor_rxtx_buffer_t, p_uart_running_rx, p_calling_action);

    return (p_uart_running_rx->p_data == p_rx_buffer->p_data &&
            p_uart_running_rx->size == p_rx_buffer->bytes);
}