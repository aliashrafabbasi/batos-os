#include "ioapic.h"

#include "acpi.h"
#include "pmm.h"

#define IOAPIC_PAGE_SIZE 4096ULL
#define IOAPIC_REGSEL_OFFSET 0x00U
#define IOAPIC_WINDOW_OFFSET 0x10U

/*
 * The IOAPIC register selector is 8 bits wide.
 *
 * Each redirection entry consumes two register numbers:
 *
 *     low  = 0x10 + index * 2
 *     high = low + 1
 *
 * Therefore the highest safely addressable entry is:
 *
 *     0x10 + index * 2 + 1 <= 0xFF
 *     index <= 119
 */
#define IOAPIC_MAX_REDIR_INDEX 119U

struct ioapic_instance
{
    uint64_t physical_address;
    volatile uint8_t *base;

    uint8_t id;
    uint8_t version;
    uint8_t max_redirection_entry;

    uint32_t gsi_base;

    uint8_t initialized;
};

static struct ioapic_instance
    ioapic_instances[IOAPIC_MAX_INSTANCES];

static uint32_t ioapic_count = 0;

/*
 * Compatibility aliases for the existing single-IOAPIC API.
 *
 * These always refer to IOAPIC instance 0.
 */
static uint64_t ioapic_physical_address = 0;
static volatile uint8_t *ioapic_base = 0;

static uint8_t ioapic_id = 0;
static uint8_t ioapic_version = 0;
static uint8_t ioapic_max_redirection_entry = 0;

static void ioapic_reset_state(void)
{
    for (uint32_t i = 0;
         i < IOAPIC_MAX_INSTANCES;
         i++)
    {
        ioapic_instances[i].physical_address = 0;
        ioapic_instances[i].base = 0;
        ioapic_instances[i].id = 0;
        ioapic_instances[i].version = 0;
        ioapic_instances[i].max_redirection_entry = 0;
        ioapic_instances[i].gsi_base = 0;
        ioapic_instances[i].initialized = 0;
    }

    ioapic_count = 0;

    ioapic_physical_address = 0;
    ioapic_base = 0;
    ioapic_id = 0;
    ioapic_version = 0;
    ioapic_max_redirection_entry = 0;
}

static volatile uint8_t *ioapic_phys_to_virt(
    uint64_t physical
)
{
    uint64_t hhdm = pmm_get_hhdm_offset();

    /*
     * IOAPIC is MMIO. It is not RAM and therefore must
     * not be passed through the PMM RAM-range validator.
     */
    if (physical > UINT64_MAX - hhdm)
    {
        return 0;
    }

    return (volatile uint8_t *)(uintptr_t)(
        physical + hhdm
    );
}

static int ioapic_select_register(uint8_t reg)
{
    if (ioapic_base == 0)
    {
        return -1;
    }

    volatile uint32_t *regsel =
        (volatile uint32_t *)(
            ioapic_base + IOAPIC_REGSEL_OFFSET
        );

    *regsel = (uint32_t)reg;

    return 0;
}

uint32_t ioapic_read_register(uint8_t reg)
{
    if (ioapic_select_register(reg) != 0)
    {
        return 0;
    }

    volatile uint32_t *window =
        (volatile uint32_t *)(
            ioapic_base + IOAPIC_WINDOW_OFFSET
        );

    return *window;
}

void ioapic_write_register(
    uint8_t reg,
    uint32_t value
)
{
    if (ioapic_select_register(reg) != 0)
    {
        return;
    }

    volatile uint32_t *window =
        (volatile uint32_t *)(
            ioapic_base + IOAPIC_WINDOW_OFFSET
        );

    *window = value;
}

static int ioapic_read_redirection_instance(
    struct ioapic_instance *instance,
    uint8_t index,
    uint64_t *value
)
{
    if (instance == 0)
        return -1;

    if (!instance->initialized ||
        instance->base == 0)
        return -2;

    if (value == 0)
        return -3;

    if (index > instance->max_redirection_entry)
        return -4;

    uint16_t low_register =
        (uint16_t)(
            IOAPIC_REDIR_BASE +
            ((uint16_t)index * 2U)
        );

    volatile uint32_t *regsel =
        (volatile uint32_t *)(
            instance->base + IOAPIC_REGSEL_OFFSET
        );

    volatile uint32_t *window =
        (volatile uint32_t *)(
            instance->base + IOAPIC_WINDOW_OFFSET
        );

    *regsel = (uint32_t)(uint8_t)low_register;
    uint32_t low = *window;

    *regsel = (uint32_t)(uint8_t)(low_register + 1U);
    uint32_t high = *window;

    *value =
        ((uint64_t)high << 32) |
        (uint64_t)low;

    return 0;
}

static int ioapic_write_redirection_instance(
    struct ioapic_instance *instance,
    uint8_t index,
    uint64_t value
)
{
    if (instance == 0)
        return -1;

    if (!instance->initialized ||
        instance->base == 0)
        return -2;

    if (index > instance->max_redirection_entry)
        return -3;

    uint16_t low_register =
        (uint16_t)(
            IOAPIC_REDIR_BASE +
            ((uint16_t)index * 2U)
        );

    volatile uint32_t *regsel =
        (volatile uint32_t *)(
            instance->base + IOAPIC_REGSEL_OFFSET
        );

    volatile uint32_t *window =
        (volatile uint32_t *)(
            instance->base + IOAPIC_WINDOW_OFFSET
        );

    /*
     * Write the high dword first while the entry remains
     * masked. The caller is responsible for preserving the
     * masked state until final routing is ready.
     */
    *regsel = (uint32_t)(uint8_t)(low_register + 1U);
    *window = (uint32_t)(value >> 32);

    *regsel = (uint32_t)(uint8_t)low_register;
    *window = (uint32_t)value;

    return 0;
}

int ioapic_read_redirection_at(
    uint32_t ioapic_index,
    uint8_t index,
    uint64_t *value
)
{
    if (ioapic_index >= ioapic_count)
        return -1;

    return ioapic_read_redirection_instance(
        &ioapic_instances[ioapic_index],
        index,
        value
    );
}

int ioapic_write_redirection_at(
    uint32_t ioapic_index,
    uint8_t index,
    uint64_t value
)
{
    if (ioapic_index >= ioapic_count)
        return -1;

    return ioapic_write_redirection_instance(
        &ioapic_instances[ioapic_index],
        index,
        value
    );
}

/*
 * Compatibility API for IOAPIC instance 0.
 */
int ioapic_read_redirection(
    uint8_t index,
    uint64_t *value
)
{
    return ioapic_read_redirection_at(
        0,
        index,
        value
    );
}

int ioapic_write_redirection(
    uint8_t index,
    uint64_t value
)
{
    return ioapic_write_redirection_at(
        0,
        index,
        value
    );
}

int ioapic_init(void)
{
    uint32_t count =
        acpi_get_madt_io_apic_count();

    if (count == 0)
    {
        return -1;
    }

    if (count > IOAPIC_MAX_INSTANCES)
    {
        return -2;
    }

    /*
     * Reset all instance state before discovery.
     */
    ioapic_reset_state();

    /*
     * Discover and validate each IOAPIC instance.
     * Each IOAPIC owns:
     *
     *     GSI base ... GSI base + max_redirection_entry
     *
     * The hardware redirection capacity is not known until
     * each IOAPIC is probed, so overlap validation is performed
     * after discovery below.
     *
     * No redirection entry is modified here.
     */
    for (uint32_t i = 0; i < count; i++)
    {
        const struct acpi_madt_io_apic *entry =
            acpi_get_madt_io_apic(i);

        if (entry == 0)
        {
            ioapic_count = 0;
            return -3;
        }

        if (entry->address == 0)
        {
            ioapic_count = 0;
            return -4;
        }

        if ((entry->address &
             (IOAPIC_PAGE_SIZE - 1ULL)) != 0)
        {
            ioapic_count = 0;
            return -5;
        }

        /*
         * Reject duplicate physical MMIO addresses.
         */
        for (uint32_t j = 0; j < i; j++)
        {
            if (ioapic_instances[j].physical_address ==
                entry->address)
            {
                ioapic_count = 0;
                return -6;
            }
        }

        volatile uint8_t *base =
            ioapic_phys_to_virt(entry->address);

        if (base == 0)
        {
            ioapic_count = 0;
            return -7;
        }

        volatile uint32_t *regsel =
            (volatile uint32_t *)(
                base + IOAPIC_REGSEL_OFFSET
            );

        volatile uint32_t *window =
            (volatile uint32_t *)(
                base + IOAPIC_WINDOW_OFFSET
            );

        /*
         * Read VERSION directly from this IOAPIC instance.
         *
         * This does not modify the redirection table.
         */
        *regsel = IOAPIC_REG_VERSION;

        uint32_t version_register = *window;

        /*
         * Read ID directly from this IOAPIC instance.
         */
        *regsel = IOAPIC_REG_ID;

        uint32_t id_register = *window;

        uint8_t version =
            (uint8_t)(
                version_register & 0xFFU
            );

        uint8_t max_redirection_entry =
            (uint8_t)(
                (version_register >> 16) & 0xFFU
            );

        /*
         * The IOREGSEL register is 8 bits wide.
         */
        if (max_redirection_entry >
            IOAPIC_MAX_REDIR_INDEX)
        {
            ioapic_count = 0;
            return -8;
        }

        if (version == 0)
        {
            ioapic_count = 0;
            return -9;
        }

        ioapic_instances[i].physical_address =
            entry->address;

        ioapic_instances[i].base =
            base;

        ioapic_instances[i].id =
            (uint8_t)(
                (id_register >> 24) & 0x0FU
            );

        ioapic_instances[i].version =
            version;

        ioapic_instances[i].max_redirection_entry =
            max_redirection_entry;

        ioapic_instances[i].gsi_base =
            entry->gsi_base;

        ioapic_instances[i].initialized = 1;

        ioapic_count++;
    }

    /*
     * Reject overlapping GSI ownership.
     *
     * Ambiguous GSI ownership must never reach the routing
     * or redirection-programming layers.
     */
    for (uint32_t i = 0; i < ioapic_count; i++)
    {
        uint64_t a_start =
            (uint64_t)ioapic_instances[i].gsi_base;

        uint64_t a_end =
            a_start +
            (uint64_t)ioapic_instances[i].max_redirection_entry;

        for (uint32_t j = i + 1; j < ioapic_count; j++)
        {
            uint64_t b_start =
                (uint64_t)ioapic_instances[j].gsi_base;

            uint64_t b_end =
                b_start +
                (uint64_t)ioapic_instances[j].max_redirection_entry;

            if (a_start <= b_end &&
                b_start <= a_end)
            {
                ioapic_reset_state();
                return -10;
            }
        }
    }

    /*
     * Preserve the original instance-0 API.
     */
    ioapic_physical_address =
        ioapic_instances[0].physical_address;

    ioapic_base =
        ioapic_instances[0].base;

    ioapic_id =
        ioapic_instances[0].id;

    ioapic_version =
        ioapic_instances[0].version;

    ioapic_max_redirection_entry =
        ioapic_instances[0].max_redirection_entry;

    return 0;
}

uint64_t ioapic_get_physical_address(void)
{
    return ioapic_physical_address;
}

uint64_t ioapic_get_virtual_address(void)
{
    return (uint64_t)(uintptr_t)ioapic_base;
}

uint8_t ioapic_get_id(void)
{
    return ioapic_id;
}

uint8_t ioapic_get_version(void)
{
    return ioapic_version;
}

uint8_t ioapic_get_max_redirection_entry(void)
{
    return ioapic_max_redirection_entry;
}


uint32_t ioapic_get_count(void)
{
    return ioapic_count;
}

uint64_t ioapic_get_physical_address_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    if (!ioapic_instances[index].initialized)
        return 0;

    return ioapic_instances[index].physical_address;
}

uint64_t ioapic_get_virtual_address_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    if (!ioapic_instances[index].initialized)
        return 0;

    return (uint64_t)(uintptr_t)(
        ioapic_instances[index].base
    );
}

uint8_t ioapic_get_id_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    return ioapic_instances[index].id;
}

uint8_t ioapic_get_version_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    return ioapic_instances[index].version;
}

uint8_t ioapic_get_max_redirection_entry_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    return ioapic_instances[index].max_redirection_entry;
}

uint32_t ioapic_get_gsi_base_at(
    uint32_t index
)
{
    if (index >= ioapic_count)
        return 0;

    return ioapic_instances[index].gsi_base;
}

int ioapic_gsi_in_range(
    uint32_t index,
    uint32_t gsi
)
{
    if (index >= ioapic_count)
        return -1;

    if (!ioapic_instances[index].initialized)
        return -2;

    uint64_t base =
        (uint64_t)ioapic_instances[index].gsi_base;

    uint64_t max_entry =
        (uint64_t)
        ioapic_instances[index].max_redirection_entry;

    uint64_t last_gsi =
        base + max_entry;

    if ((uint64_t)gsi < base)
        return 0;

    if ((uint64_t)gsi > last_gsi)
        return 0;

    return 1;
}
