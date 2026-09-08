#ifndef BATOS_LAPIC_H
#define BATOS_LAPIC_H

#include <stdint.h>

#define LAPIC_REG_ID       0x020
#define LAPIC_REG_VERSION  0x030
#define LAPIC_REG_SVR      0x0F0
#define LAPIC_REG_EOI      0x0B0

#define LAPIC_SVR_ENABLE   (1U << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFFU

int lapic_init(void);

uint64_t lapic_get_physical_address(void);
uint64_t lapic_get_virtual_address(void);

uint32_t lapic_read(uint32_t offset);
void lapic_write(uint32_t offset, uint32_t value);

void lapic_eoi(void);

#endif
