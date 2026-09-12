#include "lapic.h"
#include "acpi.h"
#include "kernel/mm/pmm/pmm.h"
#include "clock_event.h"

#define LAPIC_PAGE_SIZE 4096ULL

static uint64_t lapic_physical_address = 0;
static volatile uint8_t *lapic_base = 0;
static volatile uint64_t lapic_timer_interrupt_count = 0;

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

/*
 * Handle a Local APIC timer interrupt.
 *
 * The LAPIC timer uses its own dedicated vector
 * and therefore does not pass through irq_dispatch().
 *
 * Timekeeping and scheduler logic will be layered
 * above this primitive later.
 */
void lapic_timer_interrupt(void)
{
    lapic_timer_interrupt_count++;

    clock_event_notify();

    lapic_eoi();
}

/*
 * Configure the LAPIC timer for controlled bring-up.
 *
 * The timer is configured as a masked, one-shot timer.
 * Interrupt delivery will be enabled by a later explicit
 * step after countdown behavior has been verified.
 */
int lapic_timer_init(uint32_t initial_count)
{
    if (lapic_base == 0)
    {
        return -1;
    }

    if (initial_count == 0)
    {
        return -2;
    }

    /*
     * Divide the LAPIC timer clock by 16.
     *
     * LAPIC divide configuration encoding 0x3
     * corresponds to divide-by-16.
     */
    lapic_write(
        LAPIC_REG_TIMER_DIVIDE,
        0x3U
    );

    /*
     * Configure fixed delivery mode, vector 0xF0,
     * one-shot mode, and keep the timer masked.
     */
    lapic_write(
        LAPIC_REG_LVT_TIMER,
        LAPIC_LVT_TIMER_VECTOR |
        LAPIC_LVT_TIMER_MASK
    );

    /*
     * Writing the initial count starts the timer.
     *
     * The timer remains masked, so no timer interrupt
     * can be delivered during this controlled phase.
     */
    lapic_write(
        LAPIC_REG_TIMER_INITIAL,
        initial_count
    );

    return 0;
}

int lapic_timer_stop(void)
{
    if (lapic_base == 0)
    {
        return -1;
    }

    /*
     * Mask the timer before clearing its countdown so
     * no LAPIC timer interrupt can be delivered.
     */
    uint32_t lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    lvt |= LAPIC_LVT_TIMER_MASK;

    lapic_write(
        LAPIC_REG_LVT_TIMER,
        lvt
    );

    /*
     * Writing zero stops the countdown.
     */
    lapic_write(
        LAPIC_REG_TIMER_INITIAL,
        0
    );

    return 0;
}

uint64_t lapic_timer_get_interrupt_count(void)
{
    return lapic_timer_interrupt_count;
}

uint32_t lapic_timer_get_current_count(void)
{
    return lapic_read(
        LAPIC_REG_TIMER_CURRENT
    );
}

/*
 * Mask or unmask LAPIC timer interrupt delivery.
 *
 * This operation changes only the LVT mask bit.
 * Timer mode, vector, divider, and initial count
 * remain unchanged.
 */
int lapic_timer_set_masked(int masked)
{
    if (lapic_base == 0)
    {
        return -1;
    }

    uint32_t lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if (masked)
    {
        lvt |= LAPIC_LVT_TIMER_MASK;
    }
    else
    {
        lvt &= ~LAPIC_LVT_TIMER_MASK;
    }

    lapic_write(
        LAPIC_REG_LVT_TIMER,
        lvt
    );

    uint32_t readback =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if (masked)
    {
        if ((readback & LAPIC_LVT_TIMER_MASK) == 0)
        {
            return -2;
        }
    }
    else
    {
        if ((readback & LAPIC_LVT_TIMER_MASK) != 0)
        {
            return -3;
        }
    }

    return 0;
}

/*
 * Configure the LAPIC timer in periodic mode.
 *
 * The timer remains masked after this operation.
 * The caller explicitly enables interrupt delivery
 * only after the reload value and interrupt path
 * have been verified.
 */
int lapic_timer_configure_periodic(uint32_t initial_count)
{
    if (lapic_base == 0)
    {
        return -1;
    }

    if (initial_count == 0)
    {
        return -2;
    }

    /*
     * Stop any previous countdown before programming the
     * periodic configuration.
     */
    if (lapic_timer_stop() != 0)
    {
        return -3;
    }

    /*
     * Keep the calibrated divider at divide-by-16.
     */
    lapic_write(
        LAPIC_REG_TIMER_DIVIDE,
        0x3U
    );

    /*
     * Fixed delivery, vector 0xF0, periodic mode,
     * masked until the caller explicitly enables it.
     */
    lapic_write(
        LAPIC_REG_LVT_TIMER,
        LAPIC_LVT_TIMER_VECTOR |
        LAPIC_LVT_TIMER_PERIODIC |
        LAPIC_LVT_TIMER_MASK
    );

    /*
     * Loading the initial count starts the periodic
     * countdown, but the LVT remains masked.
     */
    lapic_write(
        LAPIC_REG_TIMER_INITIAL,
        initial_count
    );

    return 0;
}
