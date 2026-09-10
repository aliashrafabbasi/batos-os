#include "clock_event.h"
#include "time.h"

static enum clock_event_source clock_event_source =
    CLOCK_EVENT_SOURCE_NONE;

static uint32_t clock_event_frequency = 0;

static volatile uint64_t clock_event_count = 0;

int clock_event_init(
    enum clock_event_source source,
    uint32_t frequency
)
{
    if (source == CLOCK_EVENT_SOURCE_NONE)
    {
        return -1;
    }

    if (source != CLOCK_EVENT_SOURCE_PIT &&
        source != CLOCK_EVENT_SOURCE_LAPIC)
    {
        return -1;
    }

    if (frequency == 0)
    {
        return -2;
    }

    clock_event_source = source;
    clock_event_frequency = frequency;
    clock_event_count = 0;

    return 0;
}

void clock_event_notify(void)
{
    if (clock_event_source == CLOCK_EVENT_SOURCE_NONE ||
        clock_event_frequency == 0)
    {
        return;
    }

    clock_event_count++;

    time_tick();
}

enum clock_event_source clock_event_get_source(void)
{
    return clock_event_source;
}

uint32_t clock_event_get_frequency(void)
{
    return clock_event_frequency;
}

uint64_t clock_event_get_count(void)
{
    return clock_event_count;
}
