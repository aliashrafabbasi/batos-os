#ifndef BATOS_TIMER_SERVICE_H
#define BATOS_TIMER_SERVICE_H

#include <stdint.h>

void timer_service_init(void);

uint64_t timer_service_tick(void);

uint64_t timer_service_get_processed_count(void);

#endif
