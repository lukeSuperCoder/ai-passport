#include "clock_service.h"

#include <limits.h>
#include <time.h>

#define MIN_TRUSTED_UNIX_TIME 1704067200LL /* 2024-01-01 UTC */

bool clock_service_now(uint32_t *now)
{
    if (!now) return false;
    time_t current = time(NULL);
    if ((int64_t)current < MIN_TRUSTED_UNIX_TIME ||
        (uint64_t)current > UINT32_MAX) {
        return false;
    }
    *now = (uint32_t)current;
    return true;
}

