#include "timer_manager.h"

static struct timer *
timer_manager_timers[TIMER_MANAGER_MAX_TIMERS];

static uint64_t timer_manager_count = 0;

void timer_manager_init(void)
{
    for (uint64_t i = 0;
         i < TIMER_MANAGER_MAX_TIMERS;
         i++)
    {
        timer_manager_timers[i] = 0;
    }

    timer_manager_count = 0;
}

int timer_manager_add(struct timer *timer)
{
    if (timer == 0)
    {
        return -1;
    }

    /*
     * Do not register the same timer twice.
     */
    for (uint64_t i = 0;
         i < TIMER_MANAGER_MAX_TIMERS;
         i++)
    {
        if (timer_manager_timers[i] == timer)
        {
            return -1;
        }
    }

    for (uint64_t i = 0;
         i < TIMER_MANAGER_MAX_TIMERS;
         i++)
    {
        if (timer_manager_timers[i] == 0)
        {
            timer_manager_timers[i] = timer;
            timer_manager_count++;

            return 0;
        }
    }

    return -1;
}

int timer_manager_remove(struct timer *timer)
{
    if (timer == 0)
    {
        return -1;
    }

    for (uint64_t i = 0;
         i < TIMER_MANAGER_MAX_TIMERS;
         i++)
    {
        if (timer_manager_timers[i] == timer)
        {
            timer_manager_timers[i] = 0;

            if (timer_manager_count > 0)
            {
                timer_manager_count--;
            }

            return 0;
        }
    }

    return -1;
}

uint64_t timer_manager_process(void)
{
    uint64_t expired_count = 0;

    for (uint64_t i = 0;
         i < TIMER_MANAGER_MAX_TIMERS;
         i++)
    {
        struct timer *timer =
            timer_manager_timers[i];

        if (timer == 0)
        {
            continue;
        }

        if (timer_is_expired(timer) == 0)
        {
            continue;
        }

        expired_count++;

        /*
         * Explicitly rearm periodic timers.
         *
         * One-shot timers become inactive through
         * timer_rearm().
         */
        (void)timer_rearm(timer);
    }

    return expired_count;
}

uint64_t timer_manager_get_count(void)
{
    return timer_manager_count;
}
