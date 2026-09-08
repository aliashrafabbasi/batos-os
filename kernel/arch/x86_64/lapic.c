#include "lapic.h"
#include "acpi.h"
#include "pmm.h"

#define LAPIC_PAGE_SIZE 4096ULL

static uint64_t lapic_physical_address = 0;
static volatile uint8_t *lapic_base = 0;

static volatile uint8_t *lapic_phys_to_virt(
    uint64_t physical
)
{
    uint64_t hhdm = pmm_get_hhdm_offset();

    /*
     * LAPIC is an MMIO device. Do not use the PMM
     * RAM-range validator here.
     *
     * Validate only the address arithmetic needed
     * to construct the HHDM virtual address.
     */
    if (physical > UINT64_MAX - hhdm)
    {
        return 0;
    }

    return (volatile uint8_t *)(uintptr_t)(
        physical + hhdm
    );
}

uint32_t lapic_read(uint32_t offset)
{
    if (lapic_base == 0)
    {
        return 0;
    }

    volatile uint32_t *reg =
        (volatile uint32_t *)(lapic_base + offset);

    return *reg;
}

void lapic_write(
    uint32_t offset,
    uint32_t value
)
{
    if (lapic_base == 0)
    {
        return;
    }

    volatile uint32_t *reg =
        (volatile uint32_t *)(lapic_base + offset);

    *reg = value;
}

int lapic_init(void)
{
    uint32_t madt_address =
        acpi_get_madt_local_apic_address();

    if (madt_address == 0)
    {
        return -1;
    }

    /*
     * xAPIC MMIO base must be page aligned.
     */
    if ((madt_address &
         (LAPIC_PAGE_SIZE - 1)) != 0)
    {
        return -2;
    }

    lapic_physical_address =
        (uint64_t)madt_address;

    lapic_base =
        lapic_phys_to_virt(
            lapic_physical_address
        );

    if (lapic_base == 0)
    {
        lapic_physical_address = 0;
        return -3;
    }

    /*
     * Read the architectural identification
     * registers before changing LAPIC state.
     */
    uint32_t id =
        lapic_read(LAPIC_REG_ID);

    uint32_t version =
        lapic_read(LAPIC_REG_VERSION);

    /*
     * A zero version is not accepted as a valid
     * xAPIC register-space bring-up result.
     */
    if ((version & 0xFFU) == 0)
    {
        lapic_base = 0;
        lapic_physical_address = 0;
        return -4;
    }

    /*
     * Enable the Local APIC through the Spurious
     * Interrupt Vector Register.
     *
     * Preserve the remaining architectural bits,
     * but install BATOS's known spurious vector.
     */
    uint32_t svr =
        lapic_read(LAPIC_REG_SVR);

    svr &= ~0xFFU;
    svr |= LAPIC_SPURIOUS_VECTOR;
    svr |= LAPIC_SVR_ENABLE;

    lapic_write(
        LAPIC_REG_SVR,
        svr
    );

    uint32_t enabled_svr =
        lapic_read(LAPIC_REG_SVR);

    if ((enabled_svr &
         LAPIC_SVR_ENABLE) == 0)
    {
        lapic_base = 0;
        lapic_physical_address = 0;
        return -5;
    }

    /*
     * EOI is write-only/command-style for normal
     * LAPIC use. Issue one controlled EOI to verify
     * the write path without expecting readback.
     */
    (void)id;
    lapic_eoi();

    return 0;
}

uint64_t lapic_get_physical_address(void)
{
    return lapic_physical_address;
}

uint64_t lapic_get_virtual_address(void)
{
    return (uint64_t)(uintptr_t)lapic_base;
}

void lapic_eoi(void)
{
    lapic_write(
        LAPIC_REG_EOI,
        0
    );
}
