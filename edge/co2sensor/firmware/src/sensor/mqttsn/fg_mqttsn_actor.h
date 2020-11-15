#ifndef FG_MQTTSN_ACTOR_H
#define FG_MQTTSN_ACTOR_H

#include "actor/fg_actor.h"

typedef enum
{
    FG_MQTTSN_SEARCH_GATEWAY,
    FG_MQTTSN_CONNECT,
    FG_MQTTSN_DISCONNECT,
    FG_MQTTSN_PUBLISH,
} fg_mqttsn_actor_message_code_t;

typedef struct
{
    uint8_t * p_data;
    size_t size;
} fg_mqttsn_actor_publish_buffer_t;

FG_ACTOR_INTERFACE_DEC(mqttsn, fg_mqttsn_actor_message_code_t);

#endif // FG_MQTTSN_ACTOR_H__