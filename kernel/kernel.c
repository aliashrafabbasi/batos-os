void kernel_main(void)
{
    volatile char *video = (volatile char *)0xB8000;

    const char *message = "BATOS OS - Kernel Booted!";

    for (int i = 0; message[i] != '\0'; i++)
    {
        video[i * 2] = message[i];
        video[i * 2 + 1] = 0x07;
    }

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
