#include <stdint.h>

void kernel_main(void)
{
    volatile uint16_t *video = (volatile uint16_t *)0xB8000;

    const char *message = "BATOS OS - Kernel Booted!";

    for (uint64_t i = 0; message[i] != '\0'; i++)
    {
        video[i] = ((uint16_t)0x07 << 8) | message[i];
    }

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}
