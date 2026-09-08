#include "gsi.h"

#include "acpi.h"
#include "ioapic.h"

/*
 * ACPI Interrupt Source Override flags:
 *
 *   bits 1:0 = polarity
 *   bits 3:2 = trigger mode
 */
#define GSI_ISO_POLARITY_MASK 0x0003U
#define GSI_ISO_TRIGGER_MASK  0x000CU

#define GSI_ISO_POLARITY_CONFORMING 0x0000U
#define GSI_ISO_POLARITY_HIGH       0x0001U
#define GSI_ISO_POLARITY_LOW        0x0003U

#define GSI_ISO_TRIGGER_CONFORMING  0x0000U
#define GSI_ISO_TRIGGER_EDGE        0x0004U
#define GSI_ISO_TRIGGER_LEVEL       0x000CU

static uint8_t gsi_initialized = 0;

static int gsi_decode_polarity(
    uint16_t flags,
    uint8_t *polarity
)
{
    uint16_t value =
        flags & GSI_ISO_POLARITY_MASK;

    if (polarity == 0)
        return -1;

    switch (value)
    {
        case GSI_ISO_POLARITY_CONFORMING:
            *polarity = GSI_POLARITY_CONFORMING;
            return 0;

        case GSI_ISO_POLARITY_HIGH:
            *polarity = GSI_POLARITY_HIGH;
            return 0;

        case GSI_ISO_POLARITY_LOW:
            *polarity = GSI_POLARITY_LOW;
            return 0;

        default:
            return -2;
    }
}

static int gsi_decode_trigger(
    uint16_t flags,
    uint8_t *trigger_mode
)
{
    uint16_t value =
        flags & GSI_ISO_TRIGGER_MASK;

    if (trigger_mode == 0)
        return -1;

    switch (value)
    {
        case GSI_ISO_TRIGGER_CONFORMING:
            *trigger_mode = GSI_TRIGGER_CONFORMING;
            return 0;

        case GSI_ISO_TRIGGER_EDGE:
            *trigger_mode = GSI_TRIGGER_EDGE;
            return 0;

        case GSI_ISO_TRIGGER_LEVEL:
            *trigger_mode = GSI_TRIGGER_LEVEL;
            return 0;

        default:
            return -2;
    }
}

static int gsi_find_iso(
    uint8_t irq,
    const struct acpi_madt_iso **iso
)
{
    if (iso == 0)
        return -1;

    *iso = 0;

    uint32_t count =
        acpi_get_madt_iso_count();

    for (uint32_t i = 0; i < count; i++)
    {
        const struct acpi_madt_iso *entry =
            acpi_get_madt_iso(i);

        if (entry == 0)
            return -2;

        /*
         * ISO bus 0 represents the ISA bus.
         */
        if (entry->bus == 0 &&
            entry->source == irq)
        {
            *iso = entry;
            return 0;
        }
    }

    return 1;
}

int gsi_init(void)
{
    if (acpi_get_madt_io_apic_count() == 0)
        return -1;

    gsi_initialized = 1;

    return 0;
}

int gsi_resolve_gsi(
    uint32_t gsi,
    uint32_t *ioapic_index,
    uint32_t *redirection_index
)
{
    if (!gsi_initialized)
        return -1;

    if (ioapic_index == 0 ||
        redirection_index == 0)
        return -2;

    uint32_t count =
        ioapic_get_count();

    if (count == 0)
        return -3;

    /*
     * Resolve against the actual hardware redirection
     * capacity discovered for every IOAPIC instance.
     *
     * This is intentionally independent of the next
     * IOAPIC's GSI base. Each IOAPIC owns exactly:
     *
     *     GSI base ... GSI base + MAX_REDIR_ENTRY
     */
    for (uint32_t i = 0; i < count; i++)
    {
        int in_range =
            ioapic_gsi_in_range(i, gsi);

        if (in_range < 0)
            return -4;

        if (in_range == 0)
            continue;

        uint32_t base =
            ioapic_get_gsi_base_at(i);

        uint32_t resolved_redirection_index =
            gsi - base;

        /*
         * The index must fit the actual hardware entry
         * count already validated by the IOAPIC layer.
         */
        if (resolved_redirection_index >
            (uint32_t)
            ioapic_get_max_redirection_entry_at(i))
        {
            return -5;
        }

        *ioapic_index = i;
        *redirection_index =
            resolved_redirection_index;

        return 0;
    }

    return -6;
}

int gsi_get_iso_flags(
    uint8_t irq,
    uint16_t *flags
)
{
    if (!gsi_initialized)
        return -1;

    if (flags == 0)
        return -2;

    const struct acpi_madt_iso *iso = 0;

    int result =
        gsi_find_iso(irq, &iso);

    if (result < 0)
        return -3;

    if (result == 1)
        return 1;

    *flags = iso->flags;

    return 0;
}

int gsi_resolve_irq(
    uint8_t irq,
    struct gsi_irq_route *route
)
{
    if (!gsi_initialized)
        return -1;

    if (route == 0)
        return -2;

    /*
     * ISA IRQs are normally identity-mapped to GSIs.
     * ACPI ISO entries override that mapping.
     */
    uint32_t gsi = (uint32_t)irq;
    uint16_t flags = 0;
    uint8_t has_iso = 0;

    const struct acpi_madt_iso *iso = 0;

    int iso_result =
        gsi_find_iso(irq, &iso);

    if (iso_result < 0)
        return -3;

    if (iso_result == 0)
    {
        gsi = iso->gsi;
        flags = iso->flags;
        has_iso = 1;
    }

    uint8_t polarity =
        GSI_POLARITY_CONFORMING;

    uint8_t trigger_mode =
        GSI_TRIGGER_CONFORMING;

    if (has_iso)
    {
        if (gsi_decode_polarity(
                flags,
                &polarity) != 0)
        {
            return -4;
        }

        if (gsi_decode_trigger(
                flags,
                &trigger_mode) != 0)
        {
            return -5;
        }
    }

    /*
     * For ISA IRQs, ACPI "conforming" means the legacy ISA
     * defaults: active-high polarity and edge-triggered.
     */
    if (polarity == GSI_POLARITY_CONFORMING)
        polarity = GSI_POLARITY_HIGH;

    if (trigger_mode == GSI_TRIGGER_CONFORMING)
        trigger_mode = GSI_TRIGGER_EDGE;

    uint32_t ioapic_index = 0;
    uint32_t redirection_index = 0;

    if (gsi_resolve_gsi(
            gsi,
            &ioapic_index,
            &redirection_index) != 0)
    {
        return -6;
    }

    route->irq = irq;
    route->gsi = gsi;
    route->ioapic_index = ioapic_index;
    route->ioapic_redirection_index =
        redirection_index;
    route->polarity = polarity;
    route->trigger_mode = trigger_mode;
    route->has_iso = has_iso;

    return 0;
}
