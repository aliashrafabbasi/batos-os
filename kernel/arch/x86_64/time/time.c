#include "time.h"

static volatile uint64_t time_ticks = 0;
static uint32_t time_frequency = 0;

void time_init(uint32_t frequency)
{
    time_ticks = 0;
    time_frequency = frequency;
}

void time_tick(void)
{
    if (time_frequency == 0)
    {
        return;
    }

    time_ticks++;
}

uint64_t time_get_ticks(void)
{
    return time_ticks;
}

uint32_t time_get_frequency(void)
{
    return time_frequency;
}

uint64_t time_get_uptime_ms(void)
{
    if (time_frequency == 0)
    {
        return 0;
    }

    return
        (time_ticks * 1000ULL) /
        (uint64_t)time_frequency;
}
