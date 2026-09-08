#include "irq.h"
#include "pic.h"

static irq_handler_t irq_handlers[IRQ_COUNT];

static volatile uint64_t irq_ticks = 0;

static void irq0_timer_handler(struct irq_frame *frame)
{
    (void)frame;

    irq_ticks++;
}

void irq_init(void)
{
    for (uint8_t i = 0; i < IRQ_COUNT; i++)
    {
        irq_handlers[i] = 0;
    }

    /*
     * IRQ0 is the first real hardware interrupt used by BATOS.
     */
    irq_register_handler(0, irq0_timer_handler);
}

int irq_register_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq >= IRQ_COUNT)
    {
        return -1;
    }

    irq_handlers[irq] = handler;

    return 0;
}

void irq_dispatch(struct irq_frame *frame)
{
    uint64_t vector = frame->vector;

    if (vector < IRQ_VECTOR_BASE ||
        vector >= IRQ_VECTOR_BASE + IRQ_COUNT)
    {
        return;
    }

    uint8_t irq =
        (uint8_t)(vector - IRQ_VECTOR_BASE);

    irq_handler_t handler =
        irq_handlers[irq];

    if (handler != 0)
    {
        handler(frame);
    }

    /*
     * PIC EOI must be sent after the IRQ has been serviced.
     */
    pic_send_eoi(irq);
}

uint64_t irq_get_ticks(void)
{
    return irq_ticks;
}
