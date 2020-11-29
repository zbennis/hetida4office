#ifndef FG_BSP_ACTOR_H
#define FG_BSP_ACTOR_H

#include "actor/fg_actor.h"
#include "bsp.h"

typedef enum
{
    FG_BSP_ASSIGN_BUTTON,
} fg_bsp_actor_message_code_t;

typedef void (*fg_bsp_button_event_handler_t)();

typedef struct
{
    uint32_t button_no;
    bsp_button_action_t button_action;
    fg_bsp_button_event_handler_t button_event_handler;
} fg_bsp_assign_button_message_t;

FG_ACTOR_INTERFACE_DEC(bsp, fg_bsp_actor_message_code_t);

#endif // FG_BSP_ACTOR_H__