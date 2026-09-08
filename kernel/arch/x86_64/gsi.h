#ifndef BATOS_GSI_H
#define BATOS_GSI_H

#include <stdint.h>

/*
 * ACPI Interrupt Source Override polarity.
 */
#define GSI_POLARITY_CONFORMING 0U
#define GSI_POLARITY_HIGH       1U
#define GSI_POLARITY_LOW        2U

/*
 * ACPI Interrupt Source Override trigger mode.
 */
#define GSI_TRIGGER_CONFORMING  0U
#define GSI_TRIGGER_EDGE        1U
#define GSI_TRIGGER_LEVEL       2U

/*
 * Resolved routing information for an ISA IRQ.
 *
 * This is a routing description only.
 * It does not program or enable an IOAPIC entry.
 */
struct gsi_irq_route
{
    uint8_t irq;
    uint32_t gsi;

    uint32_t ioapic_index;
    uint32_t ioapic_redirection_index;

    uint8_t polarity;
    uint8_t trigger_mode;

    uint8_t has_iso;
};

int gsi_init(void);

int gsi_resolve_irq(
    uint8_t irq,
    struct gsi_irq_route *route
);

int gsi_resolve_gsi(
    uint32_t gsi,
    uint32_t *ioapic_index,
    uint32_t *redirection_index
);

int gsi_get_iso_flags(
    uint8_t irq,
    uint16_t *flags
);

#endif
