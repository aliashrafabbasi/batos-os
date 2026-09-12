#ifndef BATOS_PIT_H
#define BATOS_PIT_H

#include <stdint.h>

#define PIT_FREQUENCY 1193182U

void pit_init(uint32_t frequency);

#endif
