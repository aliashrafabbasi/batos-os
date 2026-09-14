#include "kernel/arch/x86_64/interrupt/irq_routing.h"

#include "kernel/arch/x86_64/apic/gsi.h"
#include "kernel/arch/x86_64/apic/ioapic.h"
#include "kernel/arch/x86_64/apic/lapic.h"
#include "kernel/arch/x86_64/interrupt/irq.h"
#include "kernel/arch/x86_64/interrupt/pic.h"

#define IRQ_ROUTING_TIMER_IRQ 0U
#define IRQ_ROUTING_TIMER_VECTOR \
    (IRQ_VECTOR_BASE + IRQ_ROUTING_TIMER_IRQ)

static int irq_routing_configure_timer(void)
{
    struct gsi_irq_route route;

    if (gsi_resolve_irq(
            IRQ_ROUTING_TIMER_IRQ,
            &route
        ) != 0)
    {
        return -1;
    }

    uint32_t lapic_id =
        lapic_read(LAPIC_REG_ID);

    uint8_t destination =
        (uint8_t)(lapic_id >> 24);

    uint64_t redirection =
        IRQ_ROUTING_TIMER_VECTOR |
        IOAPIC_REDIR_DELIVERY_FIXED |
        IOAPIC_REDIR_MASKED |
        ((uint64_t)destination << 56);

    if (route.polarity == GSI_POLARITY_LOW)
    {
        redirection |= IOAPIC_REDIR_POLARITY_LOW;
    }
    else if (route.polarity != GSI_POLARITY_HIGH)
    {
        return -2;
    }

    if (route.trigger_mode == GSI_TRIGGER_LEVEL)
    {
        redirection |= IOAPIC_REDIR_TRIGGER_LEVEL;
    }
    else if (route.trigger_mode != GSI_TRIGGER_EDGE)
    {
        return -3;
    }

    /*
     * The legacy PIC must not deliver IRQ0 while the IOAPIC
     * route is being established.
     */
    pic_set_mask(IRQ_ROUTING_TIMER_IRQ);

    /*
     * Keep the IOAPIC entry masked until its complete route
     * and the software controller ownership are established.
     */
    if (ioapic_write_redirection_at(
            route.ioapic_index,
            (uint8_t)route.ioapic_redirection_index,
            redirection
        ) != 0)
    {
        return -4;
    }

    uint64_t readback = 0;

    if (ioapic_read_redirection_at(
            route.ioapic_index,
            (uint8_t)route.ioapic_redirection_index,
            &readback
        ) != 0)
    {
        return -5;
    }

    if (readback != redirection)
    {
        return -6;
    }

    /*
     * irq_dispatch() must acknowledge IRQ0 through the LAPIC
     * once IOAPIC delivery is enabled.
     */
    if (irq_set_controller(
            IRQ_ROUTING_TIMER_IRQ,
            IRQ_CONTROLLER_LAPIC
        ) != 0)
    {
        return -7;
    }

    /*
     * Leave IRQ0 masked after routing ownership is established.
     *
     * The interrupt route is now structurally ready, but no
     * timer source has been activated yet. Timer-system policy
     * owns the later activation step.
     *
     * CPU interrupt-enable state remains caller-owned.
     */
    if ((readback & IOAPIC_REDIR_MASKED) == 0)
    {
        return -8;
    }

    return 0;
}

int irq_routing_init(void)
{
    /*
     * Interrupt-enable state is owned by the caller.
     * This function establishes hardware routing only.
     */
    pic_init();

    /*
     * Keep all legacy PIC delivery disabled while BATOS
     * establishes explicit IOAPIC/LAPIC ownership.
     */
    for (uint8_t irq = 0; irq < IRQ_COUNT; irq++)
    {
        pic_set_mask(irq);
    }

    irq_init();

    return irq_routing_configure_timer();
}

/*
 * Change the delivery state of the already-established IRQ0
 * IOAPIC route.
 *
 * Routing configuration and controller ownership are established
 * by irq_routing_init(). This function only changes the mask bit.
 *
 * CPU interrupt-enable state remains caller-owned.
 */
int irq_routing_set_irq0_masked(
    int masked
)
{
    struct gsi_irq_route route;

    if (gsi_resolve_irq(
            IRQ_ROUTING_TIMER_IRQ,
            &route
        ) != 0)
    {
        return -1;
    }

    uint64_t redirection = 0;

    if (ioapic_read_redirection_at(
            route.ioapic_index,
            (uint8_t)route.ioapic_redirection_index,
            &redirection
        ) != 0)
    {
        return -2;
    }

    if (masked)
    {
        redirection |= IOAPIC_REDIR_MASKED;
    }
    else
    {
        redirection &= ~IOAPIC_REDIR_MASKED;
    }

    if (ioapic_write_redirection_at(
            route.ioapic_index,
            (uint8_t)route.ioapic_redirection_index,
            redirection
        ) != 0)
    {
        return -3;
    }

    uint64_t readback = 0;

    if (ioapic_read_redirection_at(
            route.ioapic_index,
            (uint8_t)route.ioapic_redirection_index,
            &readback
        ) != 0)
    {
        return -4;
    }

    if (masked)
    {
        if ((readback & IOAPIC_REDIR_MASKED) == 0)
        {
            return -5;
        }
    }
    else
    {
        if ((readback & IOAPIC_REDIR_MASKED) != 0)
        {
            return -6;
        }
    }

    return 0;
}
