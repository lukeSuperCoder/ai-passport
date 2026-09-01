#include "services/clock_service.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define DEFAULT_SIM_TIME 22600U

bool clock_service_now(uint32_t *now)
{
    if (!now) return false;
    const char *injected = getenv("TIME_STATION_NOW");
    if (!injected || injected[0] == '\0') {
        *now = DEFAULT_SIM_TIME;
        return true;
    }
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(injected, &end, 10);
    if (errno != 0 || end == injected || *end != '\0' || value > UINT32_MAX) {
        return false;
    }
    *now = (uint32_t)value;
    return true;
}

