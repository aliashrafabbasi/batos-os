#include <stdint.h>
#include <stddef.h>

#include "kernel/boot/boot.h"
#include "kernel/platform/platform.h"
#include "kernel/arch/x86_64/cpu/cpu.h"
#include "kernel/arch/x86_64/cpu/tss.h"
#include "kernel/mm/pmm/pmm.h"
#include "kernel/mm/vmm/vmm.h"
#include "kernel/arch/x86_64/interrupt/idt.h"
#include "kernel/arch/x86_64/cpu/exception.h"
#include "kernel/arch/x86_64/interrupt/pic.h"
#include "kernel/arch/x86_64/interrupt/irq.h"
#include "kernel/arch/x86_64/apic/lapic.h"
#include "kernel/arch/x86_64/apic/ioapic.h"
#include "kernel/arch/x86_64/apic/gsi.h"
#include "kernel/arch/x86_64/time/pit.h"
#include "kernel/arch/x86_64/time/time.h"
#include "kernel/arch/x86_64/time/clock_event.h"
#include "kernel/arch/x86_64/time/timer.h"
#include "kernel/arch/x86_64/time/timer_manager.h"
#include "kernel/mm/heap/heap.h"
#include "kernel/console/console.h"
#include "kernel/tests/heap_tests.h"
#include "kernel/tests/memory_tests.h"
#include "kernel/tests/interrupt_tests.h"
#include "kernel/tests/timer_tests.h"
#include "kernel/tests/acpi_tests.h"

void kernel_main(void)
{
    boot_init();

    /*
     * CPU architecture initialization continues below.
     */

    /* --------------------------------------------------------
       CPU
       -------------------------------------------------------- */

    cpu_init();

    serial_write_string(
        "GDT READY\n"
    );

    serial_write_string(
        "TSS LOADED\n"
    );

    serial_write_string(
        "IDT READY\n"
    );

    /* --------------------------------------------------------
       TSS / IST VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nTSS/IST VERIFICATION\n"
    );

    serial_write_string(
        "TR: "
    );

    serial_write_hex(
        tss_get_selector()
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] IST: "
    );

    serial_write_hex(
        idt_get_ist(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] SELECTOR: "
    );

    serial_write_hex(
        idt_get_selector(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IST1 STACK: "
    );

    serial_write_hex(
        tss_get_ist1()
    );

    serial_write_string("\n");

    serial_write_string(
        "TSS/IST READY\n"
    );

    /* --------------------------------------------------------
       PHYSICAL MEMORY MANAGER
       -------------------------------------------------------- */

    serial_write_string(
        "\n================================\n"
    );

    serial_write_string(
        "BATOS PMM INITIALIZING...\n"
    );

    serial_write_string(
        "================================\n"
    );

    pmm_init();

    serial_write_string(
        "PMM MEMORY MAP: OK\n"
    );

    serial_write_string(
        "HHDM OFFSET: "
    );

    serial_write_hex(
        pmm_get_hhdm_offset()
    );

    serial_write_string("\n");

    serial_write_string(
        "TOTAL FRAMES: "
    );

    serial_write_hex(
        pmm_get_total_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "FREE FRAMES: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "USED FRAMES: "
    );

    serial_write_hex(
        pmm_get_used_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP PHYSICAL: "
    );

    serial_write_hex(
        pmm_get_bitmap_physical()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP SIZE: "
    );

    serial_write_hex(
        pmm_get_bitmap_size()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM READY\n"
    );

    /* --------------------------------------------------------
       ACPI RSDP DISCOVERY
       -------------------------------------------------------- */

    serial_write_string(
        "\nACPI DISCOVERY START\n"
    );

    int platform_result = platform_init();

    if (platform_result != 0)
    {
        serial_write_string(
            "PLATFORM INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "PLATFORM ERROR CODE: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-platform_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    acpi_tests_run();

    memory_tests_run();

    heap_test_dynamic_kernel_page();
    heap_test_bootstrap();
    heap_test_dynamic_page_ownership();

    interrupt_tests_lapic_bringup();

    /* --------------------------------------------------------
       LAPIC TIMER BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "LAPIC TIMER BRING-UP START\n"
    );

    /*
     * Configure a masked, one-shot LAPIC timer.
     *
     * Interrupt delivery remains disabled. The initial
     * count is intentionally large so the countdown can
     * be observed before the timer reaches zero.
     */
    const uint32_t lapic_timer_initial =
        0xFFFFFFFFU;

    int lapic_timer_result =
        lapic_timer_init(
            lapic_timer_initial
        );

    if (lapic_timer_result != 0)
    {
        serial_write_string(
            "LAPIC TIMER: INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "LAPIC TIMER ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-lapic_timer_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint32_t lapic_timer_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    uint32_t lapic_timer_divide =
        lapic_read(
            LAPIC_REG_TIMER_DIVIDE
        );

    uint32_t lapic_timer_current_before =
        lapic_read(
            LAPIC_REG_TIMER_CURRENT
        );

    serial_write_string(
        "LAPIC TIMER LVT: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_lvt
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER DIVIDE: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_divide
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER CURRENT BEFORE: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_current_before
    );

    serial_write_string("\n");

    /*
     * Give the hardware timer a short interval to count down.
     *
     * Interrupts remain disabled, so this test observes
     * only the LAPIC timer counter and cannot enter the
     * LAPIC timer interrupt handler.
     */
    for (volatile uint32_t delay = 0;
         delay < 1000000U;
         delay++)
    {
        __asm__ volatile ("pause");
    }

    uint32_t lapic_timer_current_after =
        lapic_read(
            LAPIC_REG_TIMER_CURRENT
        );

    serial_write_string(
        "LAPIC TIMER CURRENT AFTER: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_current_after
    );

    serial_write_string("\n");

    /*
     * Verify:
     *
     *   1. Timer vector is 0xF0.
     *   2. Timer remains masked.
     *   3. Periodic mode is disabled.
     *   4. Divide configuration is divide-by-16.
     *   5. Current count decreased.
     */
    uint32_t expected_lapic_timer_lvt =
        LAPIC_LVT_TIMER_VECTOR |
        LAPIC_LVT_TIMER_MASK;

    if (lapic_timer_lvt != expected_lapic_timer_lvt)
    {
        serial_write_string(
            "LAPIC TIMER LVT CONFIG: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (lapic_timer_divide != 0x3U)
    {
        serial_write_string(
            "LAPIC TIMER DIVIDE CONFIG: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (lapic_timer_current_after >=
        lapic_timer_current_before)
    {
        serial_write_string(
            "LAPIC TIMER COUNTDOWN: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "LAPIC TIMER CONFIG: VERIFIED\n"
    );

    serial_write_string(
        "LAPIC TIMER COUNTDOWN: VERIFIED\n"
    );

    serial_write_string(
        "LAPIC TIMER INTERRUPT: MASKED\n"
    );

    serial_write_string(
        "LAPIC TIMER BRING-UP: VERIFIED\n"
    );

    /*
     * Stop and reset the masked LAPIC timer after the
     * controlled countdown test. This guarantees that
     * later calibration starts from a known timer state.
     */
    int lapic_timer_stop_result =
        lapic_timer_stop();

    if (lapic_timer_stop_result != 0)
    {
        serial_write_string(
            "LAPIC TIMER STOP: FAILED\n"
        );

        serial_write_string(
            "LAPIC TIMER STOP ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-lapic_timer_stop_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "LAPIC TIMER STOP: VERIFIED\n"
    );

    interrupt_tests_run();

    /*
     * Program PIT channel 0 for 100 Hz.
     */
    pit_init(100);

    /*
     * Initialize the system timekeeping layer.
     *
     * The initial reference source is the PIT at 100 Hz.
     * Hardware IRQ0 will advance the timekeeping tick.
     */
    time_init(100);

    /*
     * Initialize the clock-event abstraction.
     *
     * The current hardware source remains the PIT at 100 Hz.
     * IRQ0 delivery is routed through the IOAPIC/LAPIC path,
     * while the clock-event layer decouples the hardware source
     * from system timekeeping.
     */
    if (clock_event_init(
            CLOCK_EVENT_SOURCE_PIT,
            100
        ) != 0)
    {
        serial_write_string(
            "CLOCK EVENT INIT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "CLOCK EVENT SOURCE: PIT\n"
    );

    serial_write_string(
        "CLOCK EVENT: READY\n"
    );

    interrupt_test_irq0_lapic_delivery();


    /* --------------------------------------------------------
       TIMER-4: LAPIC TIMER FREQUENCY CALIBRATION
       --------------------------------------------------------

       The PIT remains the temporary reference clock.

       Calibration interval:
           PIT = 100 Hz
           20 PIT ticks = 200 ms

       LAPIC timer:
           one-shot
           masked
           divide-by-16
           initial count = 0xFFFFFFFF

       The LAPIC counter decrement over the known PIT
       interval gives the measured LAPIC timer frequency.
       -------------------------------------------------------- */

    serial_write_string(
        "LAPIC TIMER CALIBRATION START\n"
    );

    /*
     * Ensure the previous LAPIC timer bring-up countdown
     * cannot participate in calibration.
     */
    if (lapic_timer_stop() != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION STOP: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    const uint32_t lapic_calibration_initial =
        0xFFFFFFFFU;

    /*
     * Configure a fresh masked one-shot countdown.
     * Interrupt delivery remains disabled during the
     * measurement interval.
     */
    if (lapic_timer_init(
            lapic_calibration_initial
        ) != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint32_t lapic_calibration_start_count =
        lapic_timer_get_current_count();

    uint64_t lapic_calibration_start_ticks =
        irq_get_ticks();

    serial_write_string(
        "LAPIC CALIBRATION START COUNT: "
    );
    serial_write_hex(
        lapic_calibration_start_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION START PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_start_ticks
    );
    serial_write_string("\n");

    /*
     * Enable only the already-proven PIT reference path.
     * The LAPIC timer remains masked.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    const uint64_t lapic_calibration_reference_ticks =
        20;

    while (
        irq_get_ticks() <
        lapic_calibration_start_ticks +
        lapic_calibration_reference_ticks
    )
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    /*
     * Stop interrupt delivery before reading the final
     * calibration state.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    uint32_t lapic_calibration_end_count =
        lapic_timer_get_current_count();

    uint64_t lapic_calibration_end_ticks =
        irq_get_ticks();

    serial_write_string(
        "LAPIC CALIBRATION END COUNT: "
    );
    serial_write_hex(
        lapic_calibration_end_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION END PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_end_ticks
    );
    serial_write_string("\n");

    /*
     * The timer must still be counting down. Reaching zero
     * would mean the selected calibration interval was too
     * long for the chosen initial count.
     */
    if (lapic_calibration_end_count == 0 ||
        lapic_calibration_end_count >=
            lapic_calibration_start_count ||
        lapic_calibration_end_ticks <=
            lapic_calibration_start_ticks)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION: FAILED\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t lapic_calibration_elapsed_count =
        (uint64_t)lapic_calibration_start_count -
        (uint64_t)lapic_calibration_end_count;

    uint64_t lapic_calibration_elapsed_pit_ticks =
        lapic_calibration_end_ticks -
        lapic_calibration_start_ticks;

    /*
     * PIT reference frequency is exactly the configured
     * 100 Hz clock-event frequency.
     *
     * Measured LAPIC frequency:
     *
     *     delta_count * PIT_HZ
     *     -------------------
     *       elapsed_ticks
     */
    const uint64_t lapic_calibration_pit_hz =
        100ULL;

    uint64_t lapic_timer_frequency =
        (
            lapic_calibration_elapsed_count *
            lapic_calibration_pit_hz
        ) /
        lapic_calibration_elapsed_pit_ticks;

    if (lapic_timer_frequency == 0 ||
        lapic_timer_frequency > 0xFFFFFFFFULL)
    {
        serial_write_string(
            "LAPIC TIMER FREQUENCY: INVALID\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Calculate the reload required for a 100 Hz periodic
     * LAPIC clock event.
     *
     * Rounded rather than truncated.
     */
    uint64_t lapic_timer_reload =
        (
            lapic_timer_frequency +
            50ULL
        ) /
        100ULL;

    if (lapic_timer_reload == 0 ||
        lapic_timer_reload > 0xFFFFFFFFULL)
    {
        serial_write_string(
            "LAPIC TIMER RELOAD: INVALID\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "LAPIC CALIBRATION ELAPSED COUNT: "
    );
    serial_write_hex(
        lapic_calibration_elapsed_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION ELAPSED PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_elapsed_pit_ticks
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER MEASURED FREQUENCY: "
    );
    serial_write_hex(
        lapic_timer_frequency
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER 100HZ RELOAD: "
    );
    serial_write_hex(
        lapic_timer_reload
    );
    serial_write_string("\n");

    /*
     * Calibration is complete. Do not switch the clock-event
     * source yet; that is the next controlled step.
     */
    if (lapic_timer_stop() != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION CLEANUP: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "LAPIC TIMER CALIBRATION: VERIFIED\n"
    );

    /*
     * --------------------------------------------------------
     * TIMER-4: LAPIC TIMER CLOCK-SOURCE MIGRATION
     *
     * The calibrated LAPIC timer becomes the active system
     * clock-event source at 100 Hz.
     *
     * The PIT hardware is kept intact for now, but its IRQ0
     * IOAPIC delivery is masked so it can no longer generate
     * system clock events.
     *
     * Ordering:
     *
     *   1. CLI
     *   2. Configure LAPIC periodic timer while masked
     *   3. Switch clock-event ownership to LAPIC
     *   4. Mask PIT IRQ0 at IOAPIC
     *   5. Verify PIT IRQ0 is masked
     *   6. Unmask LAPIC timer
     *   7. STI
     *
     * No PIT removal or hardware shutdown is performed here.
     * --------------------------------------------------------
     */

    serial_write_string(
        "TIMER-4 LAPIC CLOCK MIGRATION START\n"
    );


    struct gsi_irq_route timer4_gsi_irq0_route;

    if (gsi_resolve_irq(
            0,
            &timer4_gsi_irq0_route
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 GSI IRQ0 ROUTE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    /*
     * Program the calibrated LAPIC timer in periodic mode.
     * The timer remains masked until the complete transition
     * has been verified.
     */
    if (lapic_timer_configure_periodic(
            (uint32_t)lapic_timer_reload
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC PERIODIC CONFIG: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC PERIODIC CONFIG: VERIFIED\n"
    );

    /*
     * Switch the clock-event abstraction to the calibrated
     * LAPIC source. Timekeeping ticks are intentionally
     * preserved across the source transition.
     */
    if (clock_event_init(
            CLOCK_EVENT_SOURCE_LAPIC,
            100
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 CLOCK SOURCE SWITCH: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (clock_event_get_source() !=
            CLOCK_EVENT_SOURCE_LAPIC ||
        clock_event_get_frequency() != 100)
    {
        serial_write_string(
            "TIMER-4 CLOCK SOURCE VERIFY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 CLOCK SOURCE: LAPIC 100HZ\n"
    );

    /*
     * Mask PIT IRQ0 at the IOAPIC.
     *
     * Preserve the complete existing routing entry and change
     * only the mask bit. The ACPI-resolved GSI route is reused.
     */
    uint64_t timer4_pit_redirection = 0;

    if (ioapic_read_redirection_at(
            timer4_gsi_irq0_route.ioapic_index,
            (uint8_t)timer4_gsi_irq0_route.ioapic_redirection_index,
            &timer4_pit_redirection
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 READ: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    timer4_pit_redirection |= IOAPIC_REDIR_MASKED;

    if (ioapic_write_redirection_at(
            timer4_gsi_irq0_route.ioapic_index,
            (uint8_t)timer4_gsi_irq0_route.ioapic_redirection_index,
            timer4_pit_redirection
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 MASK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t timer4_pit_readback = 0;

    if (ioapic_read_redirection_at(
            timer4_gsi_irq0_route.ioapic_index,
            (uint8_t)timer4_gsi_irq0_route.ioapic_redirection_index,
            &timer4_pit_readback
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 READBACK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if ((timer4_pit_readback & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 MASK READBACK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 PIT IRQ0: MASKED\n"
    );

    /*
     * Enable the already-programmed periodic LAPIC timer.
     */
    if (lapic_timer_set_masked(0) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC TIMER UNMASK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC TIMER: UNMASKED\n"
    );

    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    serial_write_string(
        "TIMER-4 LAPIC CLOCK SOURCE: ACTIVE\n"
    );

    /*
     * Timer subsystem verification.
     *
     * Run after the complete LAPIC clock-source migration so
     * Timer-4 observes the live periodic LAPIC clock.
     */
    timer_tests_run();

    serial_write_string(
        "TIMER-4 LAPIC CLOCK MIGRATION: ARMED\n"
    );



    /* --------------------------------------------------------
       FINAL HALT
       -------------------------------------------------------- */

    serial_write_string(
        "CPU HALTED\n"
    );

    for (;;)
    {
        __asm__ volatile (
            "hlt"
        );
    }
}
