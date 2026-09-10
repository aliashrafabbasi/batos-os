#ifndef BATOS_TIMER_H
#define BATOS_TIMER_H

#include <stdint.h>

struct timer
{
    uint64_t deadline;
    uint64_t period;
    uint8_t active;
    uint8_t periodic;
};

void timer_init(
    struct timer *timer
);

int timer_start(
    struct timer *timer,
    uint64_t delay_ticks
);

int timer_start_periodic(
    struct timer *timer,
    uint64_t period_ticks
);

void timer_cancel(
    struct timer *timer
);

uint8_t timer_is_expired(
    const struct timer *timer
);

int timer_rearm(
    struct timer *timer
);

uint64_t timer_get_deadline(
    const struct timer *timer
);

#endif
