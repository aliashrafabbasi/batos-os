#include "pit.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43

static inline void pit_outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

void pit_init(uint32_t frequency)
{
    if (frequency == 0)
    {
        return;
    }

    uint32_t divisor =
        PIT_FREQUENCY / frequency;

    if (divisor == 0)
    {
        divisor = 1;
    }

    if (divisor > 0xFFFF)
    {
        divisor = 0xFFFF;
    }

    /*
     * Channel 0
     * Access mode: low byte then high byte
     * Mode 3: square-wave generator
     * Binary counter
     */
    pit_outb(
        PIT_COMMAND,
        0x36
    );

    pit_outb(
        PIT_CHANNEL0_DATA,
        (uint8_t)(divisor & 0xFF)
    );

    pit_outb(
        PIT_CHANNEL0_DATA,
        (uint8_t)((divisor >> 8) & 0xFF)
    );
}
