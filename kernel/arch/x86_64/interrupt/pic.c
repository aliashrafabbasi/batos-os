#include "kernel/arch/x86_64/interrupt/pic.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask  = inb(PIC_SLAVE_DATA);

    /*
     * ICW1:
     *  - begin initialization
     *  - require ICW4
     */
    outb(PIC_MASTER_COMMAND, 0x11);
    io_wait();

    outb(PIC_SLAVE_COMMAND, 0x11);
    io_wait();

    /*
     * ICW2:
     * Remap master IRQ0-7 to vectors 32-39.
     * Remap slave IRQ8-15 to vectors 40-47.
     */
    outb(PIC_MASTER_DATA, PIC_MASTER_OFFSET);
    io_wait();

    outb(PIC_SLAVE_DATA, PIC_SLAVE_OFFSET);
    io_wait();

    /*
     * ICW3:
     * Slave PIC connected to master's IRQ2.
     */
    outb(PIC_MASTER_DATA, 0x04);
    io_wait();

    outb(PIC_SLAVE_DATA, 0x02);
    io_wait();

    /*
     * ICW4:
     * 8086/88 mode.
     */
    outb(PIC_MASTER_DATA, 0x01);
    io_wait();

    outb(PIC_SLAVE_DATA, 0x01);
    io_wait();

    /*
     * Restore existing masks.
     *
     * IRQs remain masked until BATOS explicitly enables
     * an individual IRQ.
     */
    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA, slave_mask);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        outb(PIC_SLAVE_COMMAND, PIC_EOI);
    }

    outb(PIC_MASTER_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq)
{
    if (irq < 8)
    {
        uint8_t mask = inb(PIC_MASTER_DATA);
        mask |= (uint8_t)(1U << irq);
        outb(PIC_MASTER_DATA, mask);
    }
    else if (irq < 16)
    {
        uint8_t slave_irq = irq - 8;
        uint8_t mask = inb(PIC_SLAVE_DATA);
        mask |= (uint8_t)(1U << slave_irq);
        outb(PIC_SLAVE_DATA, mask);
    }
}

void pic_clear_mask(uint8_t irq)
{
    if (irq < 8)
    {
        uint8_t mask = inb(PIC_MASTER_DATA);
        mask &= (uint8_t)~(1U << irq);
        outb(PIC_MASTER_DATA, mask);
    }
    else if (irq < 16)
    {
        uint8_t slave_irq = irq - 8;
        uint8_t mask = inb(PIC_SLAVE_DATA);
        mask &= (uint8_t)~(1U << slave_irq);
        outb(PIC_SLAVE_DATA, mask);
    }
}
