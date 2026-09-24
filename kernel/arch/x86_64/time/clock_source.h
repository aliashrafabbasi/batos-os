#ifndef BATOS_CLOCK_SOURCE_H
#define BATOS_CLOCK_SOURCE_H

#include <stdint.h>

/*
 * Establish the production LAPIC system clock source at the
 * requested tick frequency.
 *
 * The LAPIC timer is calibrated against the PIT reference,
 * configured in periodic mode, selected as the clock-event
 * source, and left unmasked for normal interrupt-driven runtime.
 */
int clock_source_init(uint32_t frequency);

uint32_t clock_source_get_lapic_reload(void);

uint64_t clock_source_get_lapic_frequency(void);

#endif
