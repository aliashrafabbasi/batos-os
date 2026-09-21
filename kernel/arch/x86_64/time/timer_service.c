#include "timer_service.h"

#include "timer_manager.h"

static uint8_t timer_service_initialized = 0;
static uint64_t timer_service_processed_count = 0;

void timer_service_init(void)
{
    timer_manager_init();

    timer_service_processed_count = 0;
    timer_service_initialized = 1;
}

uint64_t timer_service_tick(void)
{
    uint64_t processed_count;

    if (timer_service_initialized == 0)
    {
        return 0;
    }

    processed_count =
        timer_manager_process();

    timer_service_processed_count +=
        processed_count;

    return processed_count;
}

uint64_t timer_service_get_processed_count(void)
{
    return timer_service_processed_count;
}
