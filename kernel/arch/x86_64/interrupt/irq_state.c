#include "irq_state.h"

uint64_t x86_64_irq_save(void)
{
    uint64_t flags;

    __asm__ volatile (
        "pushfq\n"
        "popq %0\n"
        "cli\n"
        : "=r"(flags)
        :
        : "memory", "cc"
    );

    return flags;
}

void x86_64_irq_restore(uint64_t flags)
{
    __asm__ volatile (
        "pushq %0\n"
        "popfq\n"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

int x86_64_irq_is_enabled(void)
{
    uint64_t flags;

    __asm__ volatile (
        "pushfq\n"
        "popq %0\n"
        : "=r"(flags)
        :
        : "memory"
    );

    return (flags & (1ULL << 9)) != 0;
}
