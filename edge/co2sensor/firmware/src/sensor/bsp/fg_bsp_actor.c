#include "fg_bsp_actor.h"
#include <bsp.h>
#include <nrfx.h>

/** Logging */
#define NRFX_LOG_MODULE FG_BSP_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_bsp_assign_button);

#define FG_BSP_ACTOR_SLOT_ASSIGNMENT                                                               \
    {                                                                                              \
        [FG_BSP_ASSIGN_BUTTON] = fg_bsp_assign_button                                              \
    }
#define FG_BSP_ACTOR_STATES                                                                        \
    {                                                                                              \
        BSP_UNINITIALIZED, BSP_INITIALIZED                                                         \
    }
FG_ACTOR_DEF(FG_BSP_ACTOR_SLOT_ASSIGNMENT, FG_BSP_ACTOR_STATES, FG_ACTOR_NO_TASKS);

FG_ACTOR_INTERFACE_LOCAL_DEC();
#define FG_BSP_NUM_BUTTON_ACTIONS 3
fg_bsp_button_event_handler_t m_fg_bsp_event_handlers[BUTTONS_NUMBER][FG_BSP_NUM_BUTTON_ACTIONS];

#define FG_BSP_LAST_BUTTON_EVENT BSP_EVENT_KEY_LAST


/** BSP resources */
static void bsp_event_handler(bsp_event_t event);


/** Public API */
FG_ACTOR_INIT_DEF(bsp, BSP_UNINITIALIZED, BSP_INITIALIZED, fg_bsp_init);


/** Internal implementation */
static void fg_bsp_init(void)
{
    APP_ERROR_CHECK(bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler));
    for (uint32_t button = 0; button < BUTTONS_NUMBER; button++)
    {
        bsp_event_to_button_action_assign(button, BSP_BUTTON_ACTION_PUSH, BSP_EVENT_NOTHING);
    }
}

// Map each button/action combination to a well defined button event.
static bsp_event_t assign_bsp_event_to_button_action(
    uint32_t button_no, bsp_button_action_t button_action)
{
    bsp_event_t button_event = button_no * FG_BSP_NUM_BUTTON_ACTIONS + button_action + 1;
    ASSERT(button_event <= FG_BSP_LAST_BUTTON_EVENT)
    return button_event;
}

typedef struct
{
    uint32_t button_no;
    bsp_button_action_t action;
} fg_bsp_button_action_t;

// Undo the mapping from assign_bsp_event_to_button_action().
static void get_button_action_from_bsp_event(
    bsp_event_t event, fg_bsp_button_action_t * button_action)
{
    const uint8_t action_id = event - 1;
    button_action->button_no = action_id / FG_BSP_NUM_BUTTON_ACTIONS;
    button_action->action = action_id % FG_BSP_NUM_BUTTON_ACTIONS;
}

FG_ACTOR_SLOT(fg_bsp_assign_button)
{
    FG_ACTOR_STATE_TRANSITION(BSP_INITIALIZED, BSP_INITIALIZED, "assign button");

    FG_ACTOR_GET_P_ARGS(
        fg_bsp_assign_button_message_t, p_bsp_assign_button_message, p_calling_action);
    ASSERT(p_bsp_assign_button_message->button_no >= 0)
    ASSERT(p_bsp_assign_button_message->button_no < BUTTONS_NUMBER)
    ASSERT(p_bsp_assign_button_message->button_action >= 0)
    ASSERT(p_bsp_assign_button_message->button_action < FG_BSP_NUM_BUTTON_ACTIONS)

    // Remember the event handler for the specific button/action combination.
    m_fg_bsp_event_handlers[p_bsp_assign_button_message->button_no]
                           [p_bsp_assign_button_message->button_action] =
                               p_bsp_assign_button_message->button_event_handler;
    DRVX(bsp_event_to_button_action_assign(p_bsp_assign_button_message->button_no,
        p_bsp_assign_button_message->button_action,
        assign_bsp_event_to_button_action(
            p_bsp_assign_button_message->button_no, p_bsp_assign_button_message->button_action)));
}

FG_ACTOR_INTERFACE_DEF(bsp, fg_bsp_actor_message_code_t)

static void bsp_event_handler(bsp_event_t event)
{
    if (m_fg_actor_state != BSP_INITIALIZED)
        return;

    fg_bsp_button_action_t button_action;
    get_button_action_from_bsp_event(event, &button_action);

    ASSERT(button_action.button_no >= 0);
    ASSERT(button_action.action >= 0);

    if (button_action.button_no >= BUTTONS_NUMBER ||
        button_action.action >= FG_BSP_NUM_BUTTON_ACTIONS)
        return;

    fg_bsp_button_event_handler_t event_handler =
        m_fg_bsp_event_handlers[button_action.button_no][button_action.action];
    if (event_handler != NULL)
        event_handler();
}