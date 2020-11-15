#include "i2c/fg_i2c_actor.h"
#include "gpio/fg_gpio.h"
#include "pins.h"
#include <nrfx.h>
#include <nrfx_twim.h>

/** Logging */
#define NRFX_LOG_MODULE FG_I2C_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_i2c_enable, fg_i2c_disable, fg_i2c_transact);

#define FG_I2C_ACTOR_SLOT_ASSIGNMENT                                                               \
    {                                                                                              \
        [FG_I2C_ENABLE] = fg_i2c_enable, [FG_I2C_DISABLE] = fg_i2c_disable,                        \
        [FG_I2C_TRANSACT] = fg_i2c_transact                                                        \
    }
#define FG_I2C_ACTOR_STATES                                                                        \
    {                                                                                              \
        I2C_UNINITIALIZED, I2C_OFF, I2C_IDLE, I2C_TRANSACTING                                      \
    }
FG_ACTOR_DEF(FG_I2C_ACTOR_SLOT_ASSIGNMENT, FG_I2C_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_i2c_done_evt_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();


/** I2C resources */
#define I2C_INSTANCE_ID 0

static nrfx_twim_t m_nrfx_i2c = NRFX_TWIM_INSTANCE(I2C_INSTANCE_ID);
static void i2c_irq_handler(nrfx_twim_evt_t const * p_event, void * p_context);

uint32_t m_i2c_enable_calls;


/** Public API */
FG_ACTOR_INIT_DEF(i2c, I2C_UNINITIALIZED, I2C_OFF, fg_i2c_init);


/** Internal implementation */
static void fg_i2c_init(void)
{
    fg_gpio_cfg_out_od_bidir(PIN_I2C_SDA);
    fg_gpio_cfg_out_od_bidir(PIN_I2C_SCL);
}

FG_ACTOR_SLOT(fg_i2c_enable)
{
    m_i2c_enable_calls++;

    if (m_fg_actor_state > I2C_OFF)
        return;

    FG_ACTOR_STATE_TRANSITION(I2C_OFF, I2C_IDLE, "enabling I2C peripheral");

    // TODO: Try out 400k frequency
    nrfx_twim_config_t config = NRFX_TWIM_DEFAULT_CONFIG;
    config.scl = PIN_I2C_SCL;
    config.sda = PIN_I2C_SDA;
    config.hold_bus_uninit = true;
    DRVX(nrfx_twim_init(&m_nrfx_i2c, &config, i2c_irq_handler, NULL));

    nrfx_twim_enable(&m_nrfx_i2c);
}


FG_ACTOR_SLOT(fg_i2c_disable)
{
    if (m_fg_actor_state == I2C_OFF || --m_i2c_enable_calls > 0)
        return;

    FG_ACTOR_STATE_TRANSITION(I2C_IDLE, I2C_OFF, "disabling I2C peripheral");

    // TODO: Check whether some rx/tx task is still running and if so: stop it first (+ wait for the
    // stop event, see spec 6.31.5).
    nrfx_twim_uninit(&m_nrfx_i2c); // Implicitly calls nrfx_twim_disable().
}


FG_ACTOR_SLOT(fg_i2c_transact)
{
    FG_ACTOR_STATE_TRANSITION(I2C_IDLE, I2C_TRANSACTING, "starting I2C transaction");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_i2c_done_evt_cb);

    // Retrieve the transaction struct from the message.
    FG_ACTOR_GET_P_ARGS(fg_i2c_actor_transaction_t, p_i2c_transaction, p_calling_action);

    if (p_i2c_transaction->tx_size == 0 && p_i2c_transaction->rx_size == 0)
    {
        FG_ACTOR_ERROR(p_calling_action, NRFX_ERROR_INVALID_PARAM);
        return;
    }

    // Program the I2C transaction to the peripheral's registers.
    nrfx_twim_xfer_desc_t xfer_desc;
    if (p_i2c_transaction->tx_size != 0 && p_i2c_transaction->rx_size != 0)
    {
        xfer_desc = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_TXRX(p_i2c_transaction->address,
            p_i2c_transaction->p_tx_data, p_i2c_transaction->tx_size, p_i2c_transaction->p_rx_data,
            p_i2c_transaction->rx_size);
    }
    else if (p_i2c_transaction->tx_size != 0)
    {
        xfer_desc = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_TX(
            p_i2c_transaction->address, p_i2c_transaction->p_tx_data, p_i2c_transaction->tx_size);
    }
    else if (p_i2c_transaction->rx_size != 0)
    {
        xfer_desc = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_RX(
            p_i2c_transaction->address, p_i2c_transaction->p_rx_data, p_i2c_transaction->rx_size);
    }

    nrfx_err_t result = nrfx_twim_xfer(&m_nrfx_i2c, &xfer_desc, 0);
    if (result != NRFX_SUCCESS)
    {
        FG_ACTOR_ERROR(p_calling_action, result);
    }
}

FG_ACTOR_TASK_CALLBACK(fg_i2c_done_evt_cb)
{
    FG_ACTOR_STATE_TRANSITION(I2C_TRANSACTING, I2C_IDLE, "ITC transaction finished");

    ASSERT(p_calling_action->result_size == p_completed_action->result_size);
    ASSERT(p_calling_action->p_result == p_completed_action->p_result);
}

FG_ACTOR_INTERFACE_DEF(i2c, fg_i2c_actor_message_code_t)

static bool matches_running_task(const fg_actor_action_t * const p_running_task,
    const fg_i2c_actor_transaction_t * const p_i2c_tx);

// Obs: Will be called from interrupt context.
static void i2c_irq_handler(nrfx_twim_evt_t const * p_event, void * p_context)
{
    if (m_fg_actor_state != I2C_TRANSACTING)
        return;

    fg_i2c_actor_transaction_t i2c_transaction;
    // Initialize transaction to zero to enable memcmp() equality test.
    // Obs: ... = {0} is not guaranteed to set padding to zero as well.
    memset(&i2c_transaction, 0, sizeof(i2c_transaction));

    fg_actor_action_t * const p_running_task = FG_ACTOR_GET_SINGLETON_TASK();

    const nrfx_twim_xfer_desc_t * xfer = &p_event->xfer_desc;
    switch (p_event->type)
    {
        case NRFX_TWIM_EVT_DONE:
            i2c_transaction.address = xfer->address;
            switch (xfer->type)
            {
                case NRFX_TWIM_XFER_TX:
                    i2c_transaction.p_tx_data = xfer->p_primary_buf;
                    i2c_transaction.tx_size = xfer->primary_length;
                    i2c_transaction.p_rx_data = NULL;
                    i2c_transaction.rx_size = 0;
                    break;

                case NRFX_TWIM_XFER_RX:
                    i2c_transaction.p_tx_data = NULL;
                    i2c_transaction.tx_size = 0;
                    i2c_transaction.p_rx_data = xfer->p_primary_buf;
                    i2c_transaction.rx_size = xfer->primary_length;
                    break;

                case NRFX_TWIM_XFER_TXRX:
                    i2c_transaction.p_tx_data = xfer->p_primary_buf;
                    i2c_transaction.tx_size = xfer->primary_length;
                    i2c_transaction.p_rx_data = xfer->p_secondary_buf;
                    i2c_transaction.rx_size = xfer->secondary_length;
                    break;

                default:
                    ASSERT(false)
                    break;
            }

            if (!matches_running_task(p_running_task, &i2c_transaction))
            {
                FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_INVALID_LENGTH);
            }

            p_running_task->p_result = i2c_transaction.p_rx_data;
            p_running_task->result_size = i2c_transaction.rx_size;

            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case NRFX_TWIM_EVT_DATA_NACK:
        case NRFX_TWIM_EVT_ADDRESS_NACK:
            FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_INVALID_LENGTH);
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        default:
            // ignore other interrupts
            break;
    }
}

static bool matches_running_task(const fg_actor_action_t * const p_running_task,
    const fg_i2c_actor_transaction_t * const p_i2c_tx)
{
    ASSERT(p_running_task)
    const fg_actor_transaction_t * const p_transaction = p_running_task->p_transaction;
    ASSERT(p_transaction)
    const fg_actor_action_t * const p_calling_action = p_transaction->p_calling_action;
    ASSERT(p_calling_action)

    FG_ACTOR_GET_P_ARGS(fg_i2c_actor_transaction_t, p_i2c_running_tx, p_calling_action);

    return memcmp(p_i2c_running_tx, p_i2c_tx, sizeof(fg_i2c_actor_transaction_t)) == 0;
    // No need to compare the content of the buffers as they are pointing to the same address.
}