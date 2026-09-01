#include "services/telemetry.h"

#include <stdio.h>

void telemetry_log_memory(const char *checkpoint)
{
    printf("[memory] %s (host allocator; ESP heap metrics unavailable)\n",
           checkpoint ? checkpoint : "unknown");
}

