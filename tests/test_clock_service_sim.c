#define _POSIX_C_SOURCE 200809L

#include "services/clock_service.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

int main(void)
{
    uint32_t now = 0U;
    unsetenv("TIME_STATION_NOW");
    assert(clock_service_now(&now));
    assert(now == 22600U);

    assert(setenv("TIME_STATION_NOW", "44200", 1) == 0);
    assert(clock_service_now(&now));
    assert(now == 44200U);

    assert(setenv("TIME_STATION_NOW", "invalid", 1) == 0);
    assert(!clock_service_now(&now));
    return 0;
}
