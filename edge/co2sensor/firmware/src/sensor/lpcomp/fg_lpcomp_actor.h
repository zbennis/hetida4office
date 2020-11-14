#ifndef FG_LPCOMP_ACTOR_H
#define FG_LPCOMP_ACTOR_H

#include "actor/fg_actor.h"

typedef enum { FG_LPCOMP_START, FG_LPCOMP_STOP } fg_lpcomp_actor_message_code_t;

FG_ACTOR_INTERFACE_DEC(lpcomp, fg_lpcomp_actor_message_code_t);

#endif // FG_LPCOMP_ACTOR_H__
