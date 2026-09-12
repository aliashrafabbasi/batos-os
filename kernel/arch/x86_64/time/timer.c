#include "timer.h"
#include "time.h"

void timer_init(
    struct timer *timer
)
{
    if (timer == 0)
    {
        return;
    }

    timer->deadline = 0;
    timer->period = 0;
    timer->active = 0;
    timer->periodic = 0;
}

int timer_start(
    struct timer *timer,
    uint64_t delay_ticks
)
{
    if (timer == 0)
    {
        return -1;
    }

    if (delay_ticks == 0)
    {
        return -1;
    }

    uint64_t now = time_get_ticks();

    if (delay_ticks > UINT64_MAX - now)
    {
        return -1;
    }

    timer->deadline = now + delay_ticks;
    timer->period = 0;
    timer->active = 1;
    timer->periodic = 0;

    return 0;
}

int timer_start_periodic(
    struct timer *timer,
    uint64_t period_ticks
)
{
    if (timer == 0)
    {
        return -1;
    }

    if (period_ticks == 0)
    {
        return -1;
    }

    uint64_t now = time_get_ticks();

    if (period_ticks > UINT64_MAX - now)
    {
        return -1;
    }

    timer->deadline = now + period_ticks;
    timer->period = period_ticks;
    timer->active = 1;
    timer->periodic = 1;

    return 0;
}

void timer_cancel(
    struct timer *timer
)
{
    if (timer == 0)
    {
        return;
    }

    timer->active = 0;
}

uint8_t timer_is_expired(
    const struct timer *timer
)
{
    if (timer == 0)
    {
        return 0;
    }

    if (timer->active == 0)
    {
        return 0;
    }

    return time_get_ticks() >= timer->deadline;
}

int timer_rearm(
    struct timer *timer
)
{
    if (timer == 0)
    {
        return -1;
    }

    if (timer->active == 0)
    {
        return -1;
    }

    if (timer->periodic == 0)
    {
        timer->active = 0;
        return 0;
    }

    if (timer->period == 0)
    {
        return -1;
    }

    if (timer->deadline > UINT64_MAX - timer->period)
    {
        timer->active = 0;
        return -1;
    }

    timer->deadline += timer->period;

    return 0;
}

uint64_t timer_get_deadline(
    const struct timer *timer
)
{
    if (timer == 0)
    {
        return 0;
    }

    return timer->deadline;
}
