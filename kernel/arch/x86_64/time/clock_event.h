#ifndef BATOS_CLOCK_EVENT_H
#define BATOS_CLOCK_EVENT_H

#include <stdint.h>

enum clock_event_source
{
    CLOCK_EVENT_SOURCE_NONE = 0,
    CLOCK_EVENT_SOURCE_PIT = 1,
    CLOCK_EVENT_SOURCE_LAPIC = 2
};

int clock_event_init(
    enum clock_event_source source,
    uint32_t frequency
);

void clock_event_notify(void);

enum clock_event_source clock_event_get_source(void);

uint32_t clock_event_get_frequency(void);

uint64_t clock_event_get_count(void);

#endif
