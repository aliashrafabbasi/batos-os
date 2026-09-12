#ifndef BATOS_TIMER_MANAGER_H
#define BATOS_TIMER_MANAGER_H

#include <stdint.h>

#include "timer.h"

#define TIMER_MANAGER_MAX_TIMERS 64

void timer_manager_init(void);

int timer_manager_add(struct timer *timer);

int timer_manager_remove(struct timer *timer);

uint64_t timer_manager_process(void);

uint64_t timer_manager_get_count(void);

#endif
