#ifndef BATOS_LAPIC_H
#define BATOS_LAPIC_H

#include <stdint.h>

#define LAPIC_REG_ID       0x020
#define LAPIC_REG_VERSION  0x030
#define LAPIC_REG_SVR      0x0F0
#define LAPIC_REG_EOI      0x0B0

#define LAPIC_REG_LVT_TIMER       0x320
#define LAPIC_REG_TIMER_INITIAL   0x380
#define LAPIC_REG_TIMER_CURRENT   0x390
#define LAPIC_REG_TIMER_DIVIDE    0x3E0

#define LAPIC_LVT_TIMER_VECTOR    0xF0U
#define LAPIC_LVT_TIMER_MASK      (1U << 16)
#define LAPIC_LVT_TIMER_PERIODIC  (1U << 17)

#define LAPIC_SVR_ENABLE   (1U << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFFU

int lapic_init(void);

uint64_t lapic_get_physical_address(void);
uint64_t lapic_get_virtual_address(void);

uint32_t lapic_read(uint32_t offset);
void lapic_write(uint32_t offset, uint32_t value);

void lapic_eoi(void);

/*
 * LAPIC timer interrupt entry point.
 *
 * Called directly by the dedicated assembly
 * LAPIC timer interrupt stub.
 */
void lapic_timer_interrupt(void);

uint64_t lapic_timer_get_interrupt_count(void);

/*
 * Configure the LAPIC timer for controlled bring-up.
 *
 * The timer remains masked until a later explicit
 * enable step.
 */
int lapic_timer_init(uint32_t initial_count);

#endif
