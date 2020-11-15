#include "mqttsn/fg_mqttsn_actor.h"
#include "pins.h"
#include "gpio/fg_gpio.h"
#include <nrf_assert.h>
#include <nrfx.h>

#include <openthread/thread.h>
#include <thread_utils.h>

#include <mqttsn_client.h>


/** Logging */
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_LOG_ENABLED 1
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_LOG_LEVEL 4
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_INFO_COLOR 7
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_DEBUG_COLOR 7
#define NRFX_LOG_MODULE FG_MQTTSN_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(
    fg_mqttsn_search_gateway, fg_mqttsn_connect, fg_mqttsn_disconnect, fg_mqttsn_publish);

#define FG_MQTTSN_ACTOR_SLOT_ASSIGNMENT                                                            \
    {                                                                                              \
        [FG_MQTTSN_SEARCH_GATEWAY] = fg_mqttsn_search_gateway,                                     \
        [FG_MQTTSN_CONNECT] = fg_mqttsn_connect, [FG_MQTTSN_DISCONNECT] = fg_mqttsn_disconnect,    \
        [FG_MQTTSN_PUBLISH] = fg_mqttsn_publish                                                    \
    }
#define FG_MQTTSN_ACTOR_STATES                                                                     \
    {                                                                                              \
        MQTTSN_UNINITIALIZED, MQTTSN_SEARCHING_GATEWAY, MQTTSN_CONNECTING, MQTTSN_CONNECTED,       \
            MQTTSN_PUBLISHING, MQTTSN_DISCONNECTING, MQTTSN_DISCONNECTED                          \
    }

FG_ACTOR_DEF(FG_MQTTSN_ACTOR_SLOT_ASSIGNMENT, FG_MQTTSN_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_mqttsn_gateway_found_cb, fg_mqttsn_connected_cb,
    fg_mqttsn_disconnected_cb, fg_mqttsn_published_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();

#define DEFAULT_CHILD_TIMEOUT 40 // Thread child timeout [s].
#define DEFAULT_POLL_PERIOD                                                                        \
    1000 // Thread Sleepy End Device polling period when MQTT-SN Asleep. [ms]
#define SHORT_POLL_PERIOD 100 // Thread Sleepy End Device polling period when MQTT-SN Awake. [ms]
#define SEARCH_GATEWAY_TIMEOUT 5 // MQTT-SN Gateway discovery procedure timeout in [s].

static char m_fg_mqttsn_client_id[] =
    "fg_h4o_co2sensor"; // TODO: pass in as parameter to init call.
static mqttsn_client_t m_fg_mqttsn_client;

static mqttsn_connect_opt_t m_fg_mqttsn_connect_opt;

static mqttsn_remote_t m_fg_mqttsn_gateway_addr;
static uint8_t m_fg_mqttsn_gateway_id;

static char m_fg_mqttsn_topic_name[] =
    "fg_h4o_co2sensor/measurements"; // TODO: pass in as parameter to connect message
static mqttsn_topic_t m_fg_mqttsn_topic = {
    .p_topic_name = (unsigned char *)m_fg_mqttsn_topic_name,
    .topic_id = 0,
};

static uint16_t m_fg_mqttsn_msg_id = 0;


/** Public API */
FG_ACTOR_INIT_DEF(mqttsn, MQTTSN_UNINITIALIZED, MQTTSN_DISCONNECTED, fg_mqttsn_init);


/** Internal implementation */
static void fg_mqttsn_instance_init(void);
static void fg_mqttsn_evt_handler(mqttsn_client_t * p_client, mqttsn_event_t * p_event);
static void fg_mqttsn_connect_opt_init(void);

static void fg_mqttsn_init(void)
{
    fg_mqttsn_instance_init();
    APP_ERROR_CHECK(mqttsn_client_init(&m_fg_mqttsn_client, MQTTSN_DEFAULT_CLIENT_PORT,
        fg_mqttsn_evt_handler, thread_ot_instance_get()));
    fg_mqttsn_connect_opt_init();
    fg_gpio_cfg_out_os_nopull(PIN_MQTTSN_CONNECTION_STATUS);
}

static void fg_mqttsn_state_changed_callback(uint32_t flags, void * p_context)
{
    NRFX_LOG_INFO(
        "State changed! Flags: 0x%08x Current role: %d", flags, otThreadGetDeviceRole(p_context));
}

static void fg_mqttsn_instance_init(void)
{
    thread_configuration_t thread_configuration = {
        .radio_mode = THREAD_RADIO_MODE_RX_OFF_WHEN_IDLE,
        .autocommissioning = true,
        .poll_period = DEFAULT_POLL_PERIOD,
        .default_child_timeout = DEFAULT_CHILD_TIMEOUT,
    };

    thread_init(&thread_configuration);
    thread_state_changed_callback_set(fg_mqttsn_state_changed_callback);
}

static void fg_mqttsn_connect_opt_init(void)
{
    m_fg_mqttsn_connect_opt.alive_duration = MQTTSN_DEFAULT_ALIVE_DURATION;
    m_fg_mqttsn_connect_opt.clean_session = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG;
    m_fg_mqttsn_connect_opt.will_flag = MQTTSN_DEFAULT_WILL_FLAG;
    m_fg_mqttsn_connect_opt.client_id_len = strlen(m_fg_mqttsn_client_id);
    memcpy(m_fg_mqttsn_connect_opt.p_client_id, (unsigned char *)m_fg_mqttsn_client_id,
        m_fg_mqttsn_connect_opt.client_id_len);
}

static void fg_mqttsn_sleep(void)
{
    otError error = otLinkSetPollPeriod(thread_ot_instance_get(), DEFAULT_POLL_PERIOD);
    ASSERT(error == OT_ERROR_NONE);
}

static void fg_mqttsn_wake_up(void)
{
    otError error = otLinkSetPollPeriod(thread_ot_instance_get(), SHORT_POLL_PERIOD);
    ASSERT(error == OT_ERROR_NONE);
}

FG_ACTOR_SLOT(fg_mqttsn_search_gateway)
{
    FG_ACTOR_STATE_TRANSITION(
        MQTTSN_DISCONNECTED, MQTTSN_SEARCHING_GATEWAY, "searching MQTTSN gateway");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_mqttsn_gateway_found_cb);

    fg_mqttsn_wake_up();
    DRVX(mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_gateway_found_cb)
{
    FG_ACTOR_STATE_TRANSITION(
        MQTTSN_SEARCHING_GATEWAY, MQTTSN_DISCONNECTED, "MQTTSN gateway found");
}

FG_ACTOR_SLOT(fg_mqttsn_connect)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_DISCONNECTED);

    FG_ACTOR_STATE_TRANSITION(MQTTSN_DISCONNECTED, MQTTSN_CONNECTING, "connecting MQTTSN client");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_mqttsn_connected_cb);

    fg_mqttsn_wake_up();
    DRVX(mqttsn_client_connect(&m_fg_mqttsn_client, &m_fg_mqttsn_gateway_addr,
        m_fg_mqttsn_gateway_id, &m_fg_mqttsn_connect_opt));
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_connected_cb)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_CONNECTED);

    nrf_gpio_pin_set(PIN_MQTTSN_CONNECTION_STATUS);

    FG_ACTOR_STATE_TRANSITION(
        MQTTSN_CONNECTING, MQTTSN_CONNECTED, "MQTTSN topic registered");
}

FG_ACTOR_SLOT(fg_mqttsn_disconnect)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_CONNECTED);

    FG_ACTOR_RUN_SINGLETON_TASK(fg_mqttsn_disconnected_cb);

    FG_ACTOR_STATE_TRANSITION(MQTTSN_CONNECTED, MQTTSN_DISCONNECTING, "registering MQTTSN topic");
    DRVX(mqttsn_client_disconnect(&m_fg_mqttsn_client));
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_disconnected_cb)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_DISCONNECTED);

    nrf_gpio_pin_clear(PIN_MQTTSN_CONNECTION_STATUS);

    fg_mqttsn_sleep();

    FG_ACTOR_STATE_TRANSITION(MQTTSN_DISCONNECTING, MQTTSN_DISCONNECTED, "MQTTSN disconnected");
}

FG_ACTOR_SLOT(fg_mqttsn_publish)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_CONNECTED);
    FG_ACTOR_STATE_TRANSITION(MQTTSN_CONNECTED, MQTTSN_PUBLISHING, "MQTTSN publishing");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_mqttsn_published_cb);

    FG_ACTOR_GET_P_ARGS(
        fg_mqttsn_actor_publish_buffer_t, p_mqttsn_publish_buffer, p_calling_action);
    DRVX(mqttsn_client_publish(&m_fg_mqttsn_client, m_fg_mqttsn_topic.topic_id,
        p_mqttsn_publish_buffer->p_data, p_mqttsn_publish_buffer->size, &m_fg_mqttsn_msg_id));
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_published_cb)
{
    FG_ACTOR_STATE_TRANSITION(MQTTSN_PUBLISHING, MQTTSN_CONNECTED, "MQTTSN message published");
}

FG_ACTOR_INTERFACE_DEF(mqttsn, fg_mqttsn_actor_message_code_t)

static void fg_mqttsn_evt_handler(mqttsn_client_t * p_client, mqttsn_event_t * p_event)
{
    fg_actor_action_t * const p_running_task = FG_ACTOR_GET_SINGLETON_TASK();

    switch (p_event->event_id)
    {
        case MQTTSN_EVENT_GATEWAY_FOUND:
            NRFX_LOG_INFO("MQTT-SN event: Client has found an active gateway.");
            m_fg_mqttsn_gateway_addr = *(p_event->event_data.connected.p_gateway_addr);
            m_fg_mqttsn_gateway_id = p_event->event_data.connected.gateway_id;
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case MQTTSN_EVENT_CONNECTED:
            NRFX_LOG_INFO("MQTT-SN event: Client connected.");
            uint32_t err_code = mqttsn_client_topic_register(&m_fg_mqttsn_client, m_fg_mqttsn_topic.p_topic_name,
                    strlen(m_fg_mqttsn_topic_name), &m_fg_mqttsn_msg_id);
            if (err_code != NRFX_SUCCESS)
            {
              p_running_task->error_flags |= err_code;
            }
            break;

        case MQTTSN_EVENT_DISCONNECTED:
            NRFX_LOG_INFO("MQTT-SN event: Client disconnected.");
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case MQTTSN_EVENT_REGISTERED:
            m_fg_mqttsn_topic.topic_id = p_event->event_data.registered.packet.topic.topic_id;
            NRFX_LOG_INFO("MQTT-SN event: Topic has been registered with ID: %d.",
                p_event->event_data.registered.packet.topic.topic_id);
            fg_mqttsn_sleep();
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case MQTTSN_EVENT_PUBLISHED:
            NRFX_LOG_INFO("MQTT-SN event: Client has successfully published content.");
            fg_mqttsn_sleep();
            FG_ACTOR_SINGLETON_TASK_FINISHED();
            break;

        case MQTTSN_EVENT_SLEEP_PERMIT:
            NRFX_LOG_INFO("MQTT-SN event: Client permitted to sleep.");
            fg_mqttsn_sleep();
            break;

        case MQTTSN_EVENT_SLEEP_STOP:
            NRFX_LOG_INFO("MQTT-SN event: Client wakes up.");
            fg_mqttsn_wake_up();
            break;

        case MQTTSN_EVENT_TIMEOUT:
            NRFX_LOG_INFO("MQTT-SN event: Timed-out message: %d. Message ID: %d.",
                p_event->event_data.error.msg_type, p_event->event_data.error.msg_id);
            FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_TIMEOUT);
            break;

        case MQTTSN_EVENT_SEARCHGW_TIMEOUT:
            NRFX_LOG_INFO(
                "MQTT-SN event: Gateway discovery result: 0x%x.", p_event->event_data.discovery);
            fg_mqttsn_sleep();
            FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_TIMEOUT);
            break;

        default:
            break;
    }
}