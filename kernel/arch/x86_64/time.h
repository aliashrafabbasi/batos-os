#ifndef BATOS_TIME_H
#define BATOS_TIME_H

#include <stdint.h>

void time_init(uint32_t frequency);
void time_tick(void);

uint64_t time_get_ticks(void);
uint32_t time_get_frequency(void);
uint64_t time_get_uptime_ms(void);

#endif
