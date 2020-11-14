#include "lp8/fg_lp8_actor.h"
#include "gpio/fg_gpio.h"
#include "gpio/fg_gpio_actor.h"
#include "lpcomp/fg_lpcomp_actor.h"
#include "pins.h"
#include "rtc/fg_rtc_actor.h"
#include "uart/fg_uart_actor.h"
#include <nrfx.h>
#include <stddef.h>
#include <string.h>
#include <nrf_delay.h>

/** Logging */
#define NRFX_FG_LP8_ACTOR_CONFIG_LOG_ENABLED 1
#define NRFX_FG_LP8_ACTOR_CONFIG_LOG_LEVEL 4
#define NRFX_FG_LP8_ACTOR_CONFIG_INFO_COLOR 6
#define NRFX_FG_LP8_ACTOR_CONFIG_DEBUG_COLOR 6
#define NRFX_LOG_MODULE FG_LP8_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_lp8_measure_charge);

#define FG_LP8_ACTOR_SLOT_ASSIGNMENTS                                                              \
    {                                                                                              \
        [FG_LP8_MEASURE] = fg_lp8_measure_charge                                                   \
    }
#define FG_LP8_ACTOR_STATES                                                                        \
    {                                                                                              \
        LP8_UNINITIALIZED, LP8_OFF, LP8_MEASURING, LP8_RECOVERING                                  \
    }
FG_ACTOR_DEF(FG_LP8_ACTOR_SLOT_ASSIGNMENTS, FG_LP8_ACTOR_STATES, FG_ACTOR_NO_TASKS);

// Measure result handlers
FG_ACTOR_RESULT_HANDLERS_DEC(fg_lp8_measure_saturate, fg_lp8_measure_switch_on,
    fg_lp8_measure_initialize, fg_lp8_measure_cmd, fg_lp8_measure_calculate, fg_lp8_measure_read,
    fg_lp8_measure_finalize, fg_lp8_measure_finished);

// Hard reset result handlers
FG_ACTOR_RESULT_HANDLERS_DEC(fg_lp8_hard_reset, fg_lp8_hard_reset_done);

FG_ACTOR_INTERFACE_LOCAL_DEC();

static bool m_uart_enabled;
static bool m_rtc_enabled;


/** LP8 resources */
// Delay between switchin LP8 power on and end of rising
// time, in ms.
#define LP8_PWR_ON_DELAY 2
// Time between the lcomp signal (15/16th of VDD) and the
// start of the measurement, in ms.
#define LP8_VCAP_SATURATION_TIME 500

// Number of measurements between ABC calibration.
#define LP8_CALIBRATION_CYCLES                                                                     \
    40000 // i.e. desired calibration cycle in s divided by
          // (idle time in s + VCAP saturation time in s + 0.5s) - normally >= 8 days
static uint32_t m_lp8_remaining_calibration_cycles;

#define LP8_CC_INITIAL_MEASUREMENT 0x10
#define LP8_CC_SEQUENTIAL_MEASUREMENT 0x20

#define LP8_CC_ZERO_CALIBRATION_UNFILTERED 0x40
#define LP8_CC_ZERO_CALIBRATION_FILTERED 0x41
#define LP8_CC_ZERO_CALIBRATION_UNFILTERED_RESET 0x42
#define LP8_CC_ZERO_CALIBRATION_FILTERED_RESET 0x43

#define LP8_CC_BACKGROUND_CALIBRATION_UNFILTERED 0x50
#define LP8_CC_BACKGROUND_CALIBRATION_FILTERED 0x51
#define LP8_CC_BACKGROUND_CALIBRATION_UNFILTERED_RESET 0x52
#define LP8_CC_BACKGROUND_CALIBRATION_FILTERED_RESET 0x53

#define LP8_CC_ABC 0x70
#define LP8_CC_ABC_RESET 0x72

static bool m_lp8_is_initial_measurement;

#define LP8_ERR0_FATAL_POS 0
#define LP8_ERR0_ALG_POS 2
#define LP8_ERR0_CALIBRATION_POS 3
#define LP8_ERR0_SELF_DIAG_POS 4
#define LP8_ERR0_OUT_OF_RANGE_POS 5
#define LP8_ERR0_MEMORY_POS 6
#define LP8_ERR1_VCAP1_LOW_POS 0
#define LP8_ERR1_VCAP2_LOW_POS 2
#define LP8_ERR1_ADC_POS 3

#define LP8_ERR0_FATAL_MASK 1 << LP8_ERR0_FATAL_POS
#define LP8_ERR0_ALG_MASK 1 << LP8_ERR0_ALG_POS
#define LP8_ERR0_CALIBRATION_MASK 1 << LP8_ERR0_CALIBRATION_POS
#define LP8_ERR0_SELF_DIAG_MASK 1 << LP8_ERR0_SELF_DIAG_POS
#define LP8_ERR0_OUT_OF_RANGE_MASK 1 << LP8_ERR0_OUT_OF_RANGE_POS
#define LP8_ERR0_MEMORY_MASK 1 << LP8_ERR0_MEMORY_POS
#define LP8_ERR1_VCAP1_LOW_MASK 1 << LP8_ERR1_VCAP1_LOW_POS
#define LP8_ERR1_VCAP2_LOW_MASK 1 << LP8_ERR1_VCAP2_LOW_POS
#define LP8_ERR1_ADC_MASK 1 << LP8_ERR1_ADC_POS

// All values in LP8 memory/messages are big endian (MSB first),
// signed 16 bit (S16) or unsigned 16 bit (U16), unless otherwise
// mentioned.

#define LP8_RAM_SIZE 48
#define LP8_SENSOR_STATE_SIZE 23

#define LP8_INITIAL_RAM_ADDRESS 0x0080

#define LP8_DEFAULT_PRESSURE 0x278C // 0x278C corresponds to a default pressure of 1012.4 hPa
static uint16_t m_current_pressure;

typedef struct
{
    uint8_t calculation_control;                 // 0x80
    uint8_t sensor_state[LP8_SENSOR_STATE_SIZE]; // 0x81
    uint8_t host_pressure[2]; // 0x98, externally measured air pressure, S16, in units of 10 Pa,
                              // default value: 10124 (1012.4 hPa)
    uint8_t conc[2];          // 0x9A, raw concentration value, S16, in ppm
    uint8_t conc_pc[2];       // 0x9C, pressure corrected concentration value, S16, in ppm
    uint8_t temperature[2];   // 0x9E, internally measured temperature, S16, in units of 0.01 degree
                              // Celsius
    uint8_t vcap_start[2];    // 0xA0, VCAP voltage before measurement, U16, in mV
    uint8_t vcap_end[2];      // 0xA2, VCAP voltage after measurement, U16, in mV
    uint8_t error_status[4];  // 0xA4, bit field
    uint8_t conc_filtered[2]; // 0xA8, filtered concentration value, S16, in ppm
    uint8_t
        conc_filtered_pc[2]; // 0xAA, pressure corrected filtered concentration value, S16, in ppm
    uint8_t reserved[4];     // 0xAC
} fg_lp8_ram_t;
STATIC_ASSERT(sizeof(fg_lp8_ram_t) == LP8_RAM_SIZE, "incorrect LP8 RAM size");

static uint8_t m_lp8_sensor_state[LP8_SENSOR_STATE_SIZE];

#define LP8_FUNCTION_CODE_WRITE 0x41
#define LP8_FUNCTION_CODE_WRITE_ERROR 0xC1

#define LP8_WRITE_LEN 26
#define LP8_WRITE_REQUEST_SIZE (LP8_WRITE_LEN + 4)
typedef struct
{
    uint8_t function_code;  // always LP8_FUNCTION_CODE_WRITE
    uint8_t ram_address[2]; // big endian
    uint8_t ram_len;        // in bytes, must always be LP8_WRITE_LEN
    uint8_t ram[LP8_WRITE_LEN];
} fg_lp8_write_request_t;
STATIC_ASSERT(
    sizeof(fg_lp8_write_request_t) == LP8_WRITE_REQUEST_SIZE, "incorrect LP8 write request size");

#define LP8_WRITE_RESPONSE_SIZE 1
typedef struct
{
    uint8_t function_code; // always LP8_FUNCTION_CODE_WRITE
} fg_lp8_write_response_t;
STATIC_ASSERT(sizeof(fg_lp8_write_response_t) == LP8_WRITE_RESPONSE_SIZE,
    "incorrect LP8 write response size");

#define LP8_WRITE_ERROR_SIZE 2
typedef struct
{
    uint8_t function_code; // always LP8_FUNCTION_CODE_WRITE_ERROR
    uint8_t error_code;    // not specified
} fg_lp8_write_error_t;
STATIC_ASSERT(
    sizeof(fg_lp8_write_error_t) == LP8_WRITE_ERROR_SIZE, "incorrect LP8 write error size");

#define LP8_FUNCTION_CODE_READ 0x44
#define LP8_FUNCTION_CODE_READ_ERROR 0xC4

#define LP8_READ_LEN 44
#define LP8_READ_REQUEST_SIZE 4
typedef struct
{
    uint8_t function_code;  // always LP8_FUNCTION_CODE_READ
    uint8_t ram_address[2]; // big endian
    uint8_t ram_len;        // in bytes, must always be LP8_READ_LEN
} fg_lp8_read_request_t;
STATIC_ASSERT(
    sizeof(fg_lp8_read_request_t) == LP8_READ_REQUEST_SIZE, "incorrect LP8 read request size");

#define LP8_READ_RESPONSE_SIZE (LP8_READ_LEN + 2)
typedef struct
{
    uint8_t function_code; // always LP8_FUNCTION_CODE_READ
    uint8_t ram_len;       // in bytes, must always be LP8_READ_LEN
    uint8_t ram[LP8_READ_LEN];
} fg_lp8_read_response_t;
STATIC_ASSERT(
    sizeof(fg_lp8_read_response_t) == LP8_READ_RESPONSE_SIZE, "incorrect LP8 read response size");

#define LP8_READ_ERROR_SIZE 2
typedef struct
{
    uint8_t function_code; // always LP8_FUNCTION_CODE_READ_ERROR
    uint8_t error_code;    // not specified
} fg_lp8_read_error_t;
STATIC_ASSERT(sizeof(fg_lp8_read_error_t) == LP8_READ_ERROR_SIZE, "incorrect LP8 read error size");

#define LP8_MAX_TX_PDU_SIZE MAX(LP8_WRITE_REQUEST_SIZE, LP8_READ_REQUEST_SIZE)
#define LP8_MAX_RX_PDU_SIZE                                                                        \
    MAX(LP8_WRITE_RESPONSE_SIZE,                                                                   \
        MAX(LP8_WRITE_ERROR_SIZE, MAX(LP8_READ_RESPONSE_SIZE, LP8_READ_ERROR_SIZE)))
#define LP8_MAX_PDU_SIZE MAX(LP8_MAX_TX_PDU_SIZE, LP8_MAX_RX_PDU_SIZE)

typedef union
{
    fg_lp8_write_request_t write_request;
    fg_lp8_write_response_t write_response;
    fg_lp8_write_error_t write_error;
    fg_lp8_read_request_t read_request;
    fg_lp8_read_response_t read_response;
    fg_lp8_read_error_t read_error;
} fg_lp8_modbus_pdu_t;
STATIC_ASSERT(sizeof(fg_lp8_modbus_pdu_t) == LP8_MAX_PDU_SIZE, "incorrect LP8 PDU size");

#define LP8_DEVICE_ADDRESS 0xFE // or 0x68

#define LP8_MODBUS_ADU_ADDRESS_SIZE 1
#define LP8_MODBUS_ADU_CRC_SIZE 2
#define LP8_MODBUS_ADU_OVERHEAD (LP8_MODBUS_ADU_ADDRESS_SIZE + LP8_MODBUS_ADU_CRC_SIZE)
#define LP8_MODBUS_ADU_SIZE(PDU_SIZE) ((PDU_SIZE) + LP8_MODBUS_ADU_OVERHEAD)
#define LP8_MODBUS_ADU_PDU_OFFSET LP8_MODBUS_ADU_ADDRESS_SIZE


/** UART child actor resources */
#define LP8_UART_TX_BUFFER_SIZE LP8_MODBUS_ADU_SIZE(LP8_MAX_TX_PDU_SIZE)
#define LP8_UART_RX_BUFFER_SIZE LP8_MODBUS_ADU_SIZE(LP8_MAX_RX_PDU_SIZE)

static uint8_t m_uart_tx_data[LP8_UART_TX_BUFFER_SIZE];
static fg_uart_actor_rxtx_buffer_t m_uart_tx_buffer = {.p_data = m_uart_tx_data};

static uint8_t m_uart_rx_data[LP8_UART_RX_BUFFER_SIZE];
static fg_uart_actor_rxtx_buffer_t m_uart_rx_buffer = {.p_data = m_uart_rx_data};

__STATIC_INLINE void fg_lp8_uart_reset_buffers(void)
{
    m_uart_tx_buffer.size = 0;
    m_uart_rx_buffer.size = 0;
}


/** LPCOMP child actor resources */
// none


/** GPIO child actor resources */
static const fg_gpio_pin_config_t m_lp8_rdy_pin_config = {
    .pin = PIN_LP8_MEAS_RDY, .pull = NRF_GPIO_PIN_NOPULL};


/** RTC child actor resources */
#define LP8_RTC_VCAP_SATURATION_DELAY_TICKS FG_RTC_ACTOR_MS_TO_TICKS(LP8_VCAP_SATURATION_TIME)

static const uint32_t m_saturation_delay_ticks = LP8_RTC_VCAP_SATURATION_DELAY_TICKS;


/** Public API */
FG_ACTOR_INIT_DEF(lp8, LP8_UNINITIALIZED, LP8_OFF, fg_lp8_init);


/** Implementation */
void fg_lp8_init()
{
    m_lp8_remaining_calibration_cycles = LP8_CALIBRATION_CYCLES;
    m_lp8_is_initial_measurement = true;

    nrf_gpio_cfg_default(m_lp8_rdy_pin_config.pin);

    fg_gpio_cfg_out_os_nopull(PIN_LP8_EN_PWR);
    fg_gpio_cfg_out_od_nopull(PIN_LP8_EN_REV_BLOCK);
    fg_gpio_cfg_out_os_nopull(PIN_LP8_EN_CHARGE);
    fg_gpio_cfg_out_os_nopull(PIN_LP8_EN_MEAS);

    // Initialize child actors.
    fg_uart_actor_init();
    fg_lpcomp_actor_init();
    fg_gpio_actor_init();
    fg_rtc_actor_init();
}

static void fg_send_modbus_adu(
    FG_ACTOR_RESULT_HANDLER_ARGS_DEC, fg_uart_actor_rxtx_buffer_t * tx_buffer);

FG_ACTOR_SLOT(fg_lp8_measure_charge)
{
    FG_ACTOR_STATE_TRANSITION(LP8_OFF, LP8_MEASURING, "charging");

    // Get the externally measured pressure from the message.
    FG_ACTOR_GET_ARGS(uint16_t, pressure, p_calling_action); // in units of 0.1 hPa.
    m_current_pressure = pressure ? pressure : LP8_DEFAULT_PRESSURE;

    nrf_gpio_pin_set(PIN_LP8_EN_PWR);
    nrf_delay_ms(LP8_PWR_ON_DELAY);
    nrf_gpio_pin_clear(PIN_LP8_EN_REV_BLOCK);
    nrf_gpio_pin_set(PIN_LP8_EN_CHARGE);
    FG_ACTOR_POST_MESSAGE(lpcomp, FG_LPCOMP_START);

    FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_ENABLE);
    m_rtc_enabled = true;

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_saturate);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_saturate)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (charging): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "saturating");

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_START_TIMER);
    FG_ACTOR_SET_ARGS(p_next_action, m_saturation_delay_ticks);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_switch_on);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_switch_on)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (saturating): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "switching on");

    FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_DISABLE);
    m_rtc_enabled = false;

    nrf_gpio_pin_clear(PIN_LP8_EN_CHARGE);
    nrf_gpio_pin_set(PIN_LP8_EN_MEAS);

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(gpio, FG_GPIO_LISTEN_UP);
    FG_ACTOR_SET_ARGS(p_next_action, m_lp8_rdy_pin_config);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_initialize);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_initialize)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (switching on): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "initializing");

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(gpio, FG_GPIO_LISTEN_DOWN);
    FG_ACTOR_SET_ARGS(p_next_action, m_lp8_rdy_pin_config);

    FG_ACTOR_POST_MESSAGE(uart, FG_UART_ENABLE);
    m_uart_enabled = true;

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_cmd);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_cmd)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (initializing): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "measuring");

    // Configure write request ADU buffer (not necessarily zeroed!)
    m_uart_tx_buffer.size = LP8_MODBUS_ADU_SIZE(LP8_WRITE_REQUEST_SIZE);

    // Initialize LP8 write request modbus PDU.
    fg_lp8_write_request_t * p_write_request_pdu =
        (fg_lp8_write_request_t *)(m_uart_tx_buffer.p_data + LP8_MODBUS_ADU_PDU_OFFSET);
    p_write_request_pdu->function_code = LP8_FUNCTION_CODE_WRITE;
    p_write_request_pdu->ram_address[0] = MSB_16(LP8_INITIAL_RAM_ADDRESS);
    p_write_request_pdu->ram_address[1] = LSB_16(LP8_INITIAL_RAM_ADDRESS);
    p_write_request_pdu->ram_len = LP8_WRITE_LEN;

    fg_lp8_ram_t * p_ram = (fg_lp8_ram_t *)p_write_request_pdu->ram;

    // Identify calculation control value.
    if (m_lp8_is_initial_measurement)
    {
        p_ram->calculation_control = LP8_CC_INITIAL_MEASUREMENT;
        m_lp8_is_initial_measurement = false;
    }
    else if (m_lp8_remaining_calibration_cycles == 0)
    {
        NRF_LOG_INFO("re-calibrating");
        p_ram->calculation_control = LP8_CC_ABC;
        m_lp8_remaining_calibration_cycles = LP8_CALIBRATION_CYCLES;
    }
    else
    {
        p_ram->calculation_control = LP8_CC_SEQUENTIAL_MEASUREMENT;
        m_lp8_remaining_calibration_cycles--;
    }

    // Copy prior sensor state back to sensor.
    memcpy(p_ram->sensor_state, m_lp8_sensor_state, sizeof(p_ram->sensor_state));

    p_ram->host_pressure[0] = MSB_16(m_current_pressure);
    p_ram->host_pressure[1] = LSB_16(m_current_pressure);

    fg_send_modbus_adu(FG_ACTOR_RESULT_HANDLER_ARGS, &m_uart_tx_buffer);

    // TODO: How to read error response? Split buffer for both sizes?
    m_uart_rx_buffer.size = LP8_MODBUS_ADU_SIZE(LP8_WRITE_RESPONSE_SIZE);
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(uart, FG_UART_RX);
    FG_ACTOR_SET_P_ARGS(p_next_action, fg_uart_actor_rxtx_buffer_t, &m_uart_rx_buffer);
    p_next_action->result_size = m_uart_rx_buffer.size;
    p_next_action->p_result = m_uart_rx_buffer.p_data;

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_calculate);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_calculate)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (sending meas cmd): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "calculating");

    // TODO: check for CRC error in response ADU and re-send write request in case of error
    // TODO: check for write error response in write response PDU
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(gpio, FG_GPIO_LISTEN_UP);
    FG_ACTOR_SET_ARGS(p_next_action, m_lp8_rdy_pin_config);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_read);
}

#define LP8_READ_CRC 0x3979
FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_read)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (calculating): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "reading results");

    // We use a static pre-calculated read message.
    static uint8_t m_lp8_read_adu[] = {LP8_DEVICE_ADDRESS, LP8_FUNCTION_CODE_READ,
        MSB_16(LP8_INITIAL_RAM_ADDRESS), LSB_16(LP8_INITIAL_RAM_ADDRESS), LP8_READ_LEN,
        LSB_16(LP8_READ_CRC), MSB_16(LP8_READ_CRC)};
    static const fg_uart_actor_rxtx_buffer_t m_lp8_read_tx_buffer = {
        .p_data = m_lp8_read_adu, .size = sizeof(m_lp8_read_adu)};

    fg_actor_action_t * p_next_action = FG_ACTOR_POST_MESSAGE(uart, FG_UART_TX);
    FG_ACTOR_SET_P_ARGS(p_next_action, fg_uart_actor_rxtx_buffer_t, &m_lp8_read_tx_buffer);

    // TODO: How to read error response? Split buffer for both sizes?
    m_uart_rx_buffer.size = LP8_MODBUS_ADU_SIZE(LP8_READ_RESPONSE_SIZE);
    p_next_action = FG_ACTOR_POST_MESSAGE(uart, FG_UART_RX);
    FG_ACTOR_SET_P_ARGS(p_next_action, fg_uart_actor_rxtx_buffer_t, &m_uart_rx_buffer);
    p_next_action->result_size = m_uart_rx_buffer.size;
    p_next_action->p_result = m_uart_rx_buffer.p_data;

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_finalize);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_finalize)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (reading results): %#x!");

    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_MEASURING, "finalizing");

    FG_ACTOR_POST_MESSAGE(uart, FG_UART_DISABLE);
    m_uart_enabled = false;

    fg_actor_action_t * const p_completed_tx_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    fg_actor_action_t * const p_completed_rx_action =
        p_completed_tx_action->p_next_concurrent_action;
    ASSERT(p_completed_rx_action)
    ASSERT(p_completed_rx_action->message.code == FG_UART_RX)
    FG_ACTOR_GET_P_RESULT_ARR(uint8_t, LP8_MODBUS_ADU_SIZE(LP8_READ_RESPONSE_SIZE),
        lp8_read_response_adu, p_completed_rx_action);
    // TODO: check for CRC error in response ADU and re-send read request in case of error

    fg_lp8_read_response_t * p_read_response_pdu =
        (fg_lp8_read_response_t *)&lp8_read_response_adu[LP8_MODBUS_ADU_PDU_OFFSET];
    // TODO: check for read error response in read response PDU
    ASSERT(p_read_response_pdu->ram_len == LP8_READ_LEN);

    fg_lp8_ram_t * p_ram = (fg_lp8_ram_t *)p_read_response_pdu->ram;
    memcpy(m_lp8_sensor_state, p_ram->sensor_state, sizeof(m_lp8_sensor_state));

    const fg_actor_action_t * const p_calling_action = p_completed_transaction->p_calling_action;
    ASSERT(p_calling_action)
    ASSERT(p_calling_action->result_size == sizeof(fg_lp8_measurement_t))
    ASSERT(p_calling_action->p_result)

    fg_lp8_measurement_t * const p_measurement = p_calling_action->p_result;
    p_measurement->conc = ((int16_t)p_ram->conc[0] << 8) + p_ram->conc[1];
    p_measurement->conc_filtered =
        ((int16_t)p_ram->conc_filtered[0] << 8) + p_ram->conc_filtered[1];
    p_measurement->conc_pc = ((int16_t)p_ram->conc_pc[0] << 8) + p_ram->conc_pc[1];
    p_measurement->conc_filtered_pc =
        ((int16_t)p_ram->conc_filtered_pc[0] << 8) + p_ram->conc_filtered_pc[1];
    p_measurement->temperature = ((int16_t)p_ram->temperature[0] << 8) + p_ram->temperature[1];

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_measure_finished);
}

static void fg_lp8_shutdown();

FG_ACTOR_RESULT_HANDLER(fg_lp8_measure_finished)
{
    RESET_ON_ERROR(fg_lp8_hard_reset, "measuring (disabling peripherals): %#x!");
    fg_lp8_shutdown();
    FG_ACTOR_STATE_TRANSITION(LP8_MEASURING, LP8_OFF, "measurement finished");
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_hard_reset)
{
    if (m_fg_actor_state == LP8_RECOVERING)
        return;

    // Make sure we've not yet started another transaction.
    ASSERT(!p_next_transaction->p_first_concurrent_action);
    ASSERT(!p_next_transaction->result_handler);

    m_fg_actor_state = LP8_RECOVERING;
    NRFX_LOG_WARNING("hard reset");

    // Try to force a well-defined actor state.
    fg_lp8_uart_reset_buffers();
    fg_lp8_shutdown();

    FG_ACTOR_POST_MESSAGE(lpcomp, FG_LPCOMP_STOP);

    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(gpio, FG_GPIO_STOP);
    FG_ACTOR_SET_ARGS(p_next_action, m_lp8_rdy_pin_config);

    if (m_rtc_enabled)
    {
        FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_DISABLE);
        m_rtc_enabled = false;
    }
    if (m_uart_enabled)
    {
        FG_ACTOR_POST_MESSAGE(uart, FG_UART_DISABLE);
        m_uart_enabled = false;
    }

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(fg_lp8_hard_reset_done);
}

FG_ACTOR_RESULT_HANDLER(fg_lp8_hard_reset_done)
{
    ASSERT(!FG_ACTOR_GET_FIRST_COMPLETED_ACTION()->error_flags)

    FG_ACTOR_STATE_TRANSITION(LP8_RECOVERING, LP8_OFF, "recovered");
}

static void fg_lp8_shutdown() {
    nrf_gpio_pin_clear(PIN_LP8_EN_MEAS);
    nrf_gpio_pin_clear(PIN_LP8_EN_CHARGE);
    nrf_gpio_pin_set(PIN_LP8_EN_REV_BLOCK);
    nrf_gpio_pin_clear(PIN_LP8_EN_PWR);
}

static uint16_t calc_modbus_crc(uint8_t * buf, int len);

static void fg_send_modbus_adu(
    FG_ACTOR_RESULT_HANDLER_ARGS_DEC, fg_uart_actor_rxtx_buffer_t * tx_buffer)
{
    tx_buffer->p_data[0] = LP8_DEVICE_ADDRESS;

    // calculate CRC
    uint16_t crc = calc_modbus_crc(tx_buffer->p_data, tx_buffer->size - LP8_MODBUS_ADU_CRC_SIZE);
    size_t crc_offset = tx_buffer->size - LP8_MODBUS_ADU_CRC_SIZE;
    tx_buffer->p_data[crc_offset] = LSB_16(crc);
    tx_buffer->p_data[crc_offset + 1] = MSB_16(crc);

    // send over UART
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(uart, FG_UART_TX);
    FG_ACTOR_SET_P_ARGS(p_next_action, fg_uart_actor_rxtx_buffer_t, tx_buffer);
}

static uint16_t calc_modbus_crc(uint8_t * buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos]; // XOR byte into least sig. byte of crc

        for (int i = 8; i > 0; i--) // Loop over each bit
        {
            if ((crc & 0x0001) != 0) // If the LSB is set
            {
                crc >>= 1; // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else // Else LSB is not set
            {
                crc >>= 1; // Just shift right
            }
        }
    }

    return crc;
}

FG_ACTOR_INTERFACE_DEF(lp8, fg_lp8_actor_message_code_t)