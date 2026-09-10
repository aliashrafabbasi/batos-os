#include "irq.h"
#include "pic.h"
#include "lapic.h"
#include "clock_event.h"

static irq_handler_t irq_handlers[IRQ_COUNT];

static enum irq_controller irq_controllers[IRQ_COUNT];

static volatile uint64_t irq_ticks = 0;

static void irq0_timer_handler(struct irq_frame *frame)
{
    (void)frame;

    irq_ticks++;
    clock_event_notify();
}

void irq_init(void)
{
    for (uint8_t i = 0; i < IRQ_COUNT; i++)
    {
        irq_handlers[i] = 0;
        irq_controllers[i] = IRQ_CONTROLLER_PIC;
    }

    /*
     * IRQ0 is the first real hardware interrupt used by BATOS.
     *
     * The IRQ0 handler is registered here.
     *
     * The live timer source is currently PIT-driven, with
     * IRQ0 delivered through the IOAPIC to the LAPIC.
     * The clock-event abstraction sits between IRQ delivery
     * and timekeeping.
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

int irq_set_controller(
    uint8_t irq,
    enum irq_controller controller
)
{
    if (irq >= IRQ_COUNT)
    {
        return -1;
    }

    if (controller != IRQ_CONTROLLER_PIC &&
        controller != IRQ_CONTROLLER_LAPIC)
    {
        return -1;
    }

    irq_controllers[irq] = controller;

    return 0;
}

enum irq_controller irq_get_controller(uint8_t irq)
{
    if (irq >= IRQ_COUNT)
    {
        return IRQ_CONTROLLER_PIC;
    }

    return irq_controllers[irq];
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
     * Complete the interrupt using the controller that
     * delivered this IRQ.
     *
     * Controller selection is explicit rather than inferred
     * from the interrupt vector. PIC and LAPIC may use the
     * same vector space.
     */
    switch (irq_controllers[irq])
    {
        case IRQ_CONTROLLER_PIC:
            pic_send_eoi(irq);
            break;

        case IRQ_CONTROLLER_LAPIC:
            lapic_eoi();
            break;

        default:
            /*
             * irq_set_controller() prevents invalid values.
             * Keep a defensive fallback to the legacy PIC path.
             */
            pic_send_eoi(irq);
            break;
    }
}

uint64_t irq_get_ticks(void)
{
    return irq_ticks;
}
