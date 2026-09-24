#include "clock_source.h"

#include "clock_event.h"
#include "../interrupt/irq_routing.h"
#include "../apic/lapic.h"
#include "pit.h"
#include "time.h"

#include "../interrupt/irq.h"
#include "../interrupt/irq_state.h"
#include "../../../console/console.h"

#include <stdint.h>

static uint64_t clock_source_lapic_frequency = 0;
static uint32_t clock_source_lapic_reload = 0;

static int clock_source_calibrate_lapic(
    uint32_t reference_frequency,
    uint32_t *reload,
    uint64_t *measured_frequency
)
{
    uint64_t irq_flags;
    uint64_t start_pit_ticks;
    uint32_t start_count;
    uint32_t end_count;
    uint64_t end_pit_ticks;
    uint64_t elapsed_count;
    uint64_t elapsed_pit_ticks;
    uint64_t lapic_frequency;
    uint64_t lapic_reload;

    if (reference_frequency == 0 ||
        reload == NULL ||
        measured_frequency == NULL)
    {
        return -1;
    }

    irq_flags = x86_64_irq_save();

    /*
     * The production calibration uses PIT only as a temporary
     * reference. The LAPIC timer is stopped so it cannot
     * contribute clock events during measurement.
     */
    if (lapic_timer_stop() != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -2;
    }

    pit_init(reference_frequency);

    time_init(reference_frequency);

    if (clock_event_init(
            CLOCK_EVENT_SOURCE_PIT,
            reference_frequency
        ) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -3;
    }

    if (irq_routing_set_irq0_masked(0) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -4;
    }

    if (lapic_timer_init(0xFFFFFFFFU) != 0)
    {
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -5;
    }

    start_count =
        lapic_timer_get_current_count();

    start_pit_ticks =
        irq_get_irq0_delivery_count();

    /*
     * Temporarily enable the PIT reference interrupt.
     * Scheduler/preemption are still disabled at this stage.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    const uint64_t reference_ticks = 20;

    while (
        irq_get_irq0_delivery_count() <
        start_pit_ticks + reference_ticks
    )
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    end_count =
        lapic_timer_get_current_count();

    end_pit_ticks =
        irq_get_irq0_delivery_count();

    if (end_count == 0 ||
        end_count >= start_count ||
        end_pit_ticks <= start_pit_ticks)
    {
        lapic_timer_stop();
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -6;
    }

    elapsed_count =
        (uint64_t)start_count -
        (uint64_t)end_count;

    elapsed_pit_ticks =
        end_pit_ticks -
        start_pit_ticks;

    if (elapsed_pit_ticks == 0)
    {
        lapic_timer_stop();
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -7;
    }

    lapic_frequency =
        (
            elapsed_count *
            (uint64_t)reference_frequency
        ) /
        elapsed_pit_ticks;

    if (lapic_frequency == 0 ||
        lapic_frequency > 0xFFFFFFFFULL)
    {
        lapic_timer_stop();
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -8;
    }

    lapic_reload =
        (
            lapic_frequency +
            ((uint64_t)reference_frequency / 2ULL)
        ) /
        (uint64_t)reference_frequency;

    if (lapic_reload == 0 ||
        lapic_reload > 0xFFFFFFFFULL)
    {
        lapic_timer_stop();
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -9;
    }

    if (lapic_timer_stop() != 0)
    {
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -10;
    }

    if (lapic_timer_configure_periodic(
            (uint32_t)lapic_reload
        ) != 0)
    {
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -11;
    }

    if (clock_event_init(
            CLOCK_EVENT_SOURCE_LAPIC,
            reference_frequency
        ) != 0)
    {
        lapic_timer_stop();
        irq_routing_set_irq0_masked(1);
        x86_64_irq_restore(irq_flags);
        return -12;
    }

    if (irq_routing_set_irq0_masked(1) != 0)
    {
        lapic_timer_stop();
        x86_64_irq_restore(irq_flags);
        return -13;
    }

    if (lapic_timer_set_masked(0) != 0)
    {
        lapic_timer_stop();
        x86_64_irq_restore(irq_flags);
        return -14;
    }

    *reload =
        (uint32_t)lapic_reload;

    *measured_frequency =
        lapic_frequency;

    x86_64_irq_restore(irq_flags);

    return 0;
}

int clock_source_init(uint32_t frequency)
{
    uint32_t reload = 0;
    uint64_t measured_frequency = 0;

    if (frequency == 0)
        return -1;

    if (clock_source_calibrate_lapic(
            frequency,
            &reload,
            &measured_frequency
        ) != 0)
    {
        return -2;
    }

    clock_source_lapic_reload = reload;
    clock_source_lapic_frequency = measured_frequency;

    serial_write_string(
        "CLOCK SOURCE: LAPIC ACTIVE\n"
    );

    serial_write_string(
        "CLOCK SOURCE LAPIC FREQUENCY: "
    );
    serial_write_hex(
        measured_frequency
    );
    serial_write_string("\n");

    serial_write_string(
        "CLOCK SOURCE LAPIC RELOAD: "
    );
    serial_write_hex(
        reload
    );
    serial_write_string("\n");

    return 0;
}

uint32_t clock_source_get_lapic_reload(void)
{
    return clock_source_lapic_reload;
}

uint64_t clock_source_get_lapic_frequency(void)
{
    return clock_source_lapic_frequency;
}
