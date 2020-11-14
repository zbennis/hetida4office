#include "app_util.h"

#define LOG_1(n) (((n) >= 2) ? 1 : 0)
#define LOG_2(n) (((n) >= 1 << 2) ? (2 + LOG_1((n) >> 2)) : LOG_1(n))
#define LOG_4(n) (((n) >= 1 << 4) ? (4 + LOG_2((n) >> 4)) : LOG_2(n))
#define LOG_8(n) (((n) >= 1 << 8) ? (8 + LOG_4((n) >> 8)) : LOG_4(n))
#define LOG(n) (((n) >= 1 << 16) ? (16 + LOG_8((n) >> 16)) : LOG_8(n))

#define NEXT_LARGEST_POWER_OF_TWO(n) (IS_POWER_OF_TWO(n) ? (n) : 1 << (LOG(n) + 1))
