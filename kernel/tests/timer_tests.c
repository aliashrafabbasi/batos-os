#include <stdint.h>

#include "kernel/arch/x86_64/apic/lapic.h"
#include "kernel/arch/x86_64/apic/gsi.h"
#include "kernel/arch/x86_64/time.h"
#include "kernel/arch/x86_64/clock_event.h"
#include "kernel/arch/x86_64/timer.h"
#include "kernel/arch/x86_64/timer_manager.h"
#include "kernel/console/console.h"
#include "kernel/tests/timer_tests.h"

void timer_tests_run(void)
{
    serial_write_string(
        "TIMER-1 TEST START\n"
    );

    struct timer one_shot_timer;
    struct timer periodic_timer;

    timer_init(&one_shot_timer);
    timer_init(&periodic_timer);

    /*
     * Basic initialization.
     */
    if (timer_get_deadline(&one_shot_timer) != 0 ||
        timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 INIT: VERIFIED\n"
    );

    /*
     * Zero-delay one-shot must be rejected.
     */
    if (timer_start(&one_shot_timer, 0) == 0)
    {
        serial_write_string(
            "TIMER-1 ZERO DELAY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Start a real one-shot timer for 5 PIT timekeeping ticks.
     */
    uint64_t one_shot_start =
        time_get_ticks();

    if (timer_start(&one_shot_timer, 5) != 0)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t one_shot_deadline =
        timer_get_deadline(&one_shot_timer);

    if (one_shot_deadline !=
        one_shot_start + 5)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * It must not be expired immediately after starting.
     */
    if (timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PRE-DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 ONE-SHOT START: VERIFIED\n"
    );

    /*
     * Wait until the real timekeeping clock reaches
     * the one-shot deadline.
     */
    while (time_get_ticks() < one_shot_deadline)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&one_shot_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 EXPIRY: VERIFIED\n"
    );

    /*
     * Rearming a one-shot timer completes/deactivates it.
     */
    if (timer_rearm(&one_shot_timer) != 0 ||
        timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 ONE-SHOT REARM: VERIFIED\n"
    );

    /*
     * Periodic timer: first deadline must be one period
     * after the current clock.
     */
    uint64_t periodic_start =
        time_get_ticks();

    if (timer_start_periodic(&periodic_timer, 3) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t periodic_deadline_1 =
        timer_get_deadline(&periodic_timer);

    if (periodic_deadline_1 !=
        periodic_start + 3)
    {
        serial_write_string(
            "TIMER-1 PERIODIC DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC START: VERIFIED\n"
    );

    /*
     * Wait for first periodic expiry.
     */
    while (time_get_ticks() < periodic_deadline_1)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&periodic_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Rearm must advance exactly one period.
     */
    if (timer_rearm(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t periodic_deadline_2 =
        timer_get_deadline(&periodic_timer);

    if (periodic_deadline_2 !=
        periodic_deadline_1 + 3 ||
        timer_is_expired(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC REARM: VERIFIED\n"
    );

    /*
     * Verify a second periodic cycle.
     */
    while (time_get_ticks() < periodic_deadline_2)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&periodic_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC SECOND EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_rearm(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC SECOND REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC SECOND CYCLE: VERIFIED\n"
    );

    /*
     * Cancellation must make an active timer non-expiring.
     */
    timer_cancel(&periodic_timer);

    if (timer_is_expired(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 CANCEL: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 CANCEL: VERIFIED\n"
    );

    /*
     * Zero-period periodic timer must be rejected.
     */
    if (timer_start_periodic(&periodic_timer, 0) == 0)
    {
        serial_write_string(
            "TIMER-1 ZERO PERIOD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 INVALID INPUTS: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-1 SOFTWARE TIMER: VERIFIED\n"
    );

    /* --------------------------------------------------------
       TIMER-2 TIMER MANAGER / EXPIRY ENGINE
       -------------------------------------------------------- */

    serial_write_string(
        "TIMER-2 TEST START\n"
    );

    struct timer manager_one_shot;
    struct timer manager_periodic;

    timer_manager_init();

    /*
     * A fresh manager must start empty.
     */
    if (timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 INIT: VERIFIED\n"
    );

    /*
     * NULL registration must be rejected.
     */
    if (timer_manager_add(0) == 0)
    {
        serial_write_string(
            "TIMER-2 NULL ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Prepare a real one-shot timer.
     */
    timer_init(&manager_one_shot);

    if (timer_start(&manager_one_shot, 3) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Registration must succeed exactly once.
     */
    if (timer_manager_add(&manager_one_shot) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_get_count() != 1)
    {
        serial_write_string(
            "TIMER-2 COUNT AFTER ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Duplicate registration must be rejected.
     */
    if (timer_manager_add(&manager_one_shot) == 0)
    {
        serial_write_string(
            "TIMER-2 DUPLICATE ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 REGISTRATION: VERIFIED\n"
    );

    /*
     * Before the deadline, the manager must report
     * no expired timers.
     */
    if (timer_manager_process() != 0)
    {
        serial_write_string(
            "TIMER-2 PRE-EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Wait for the actual PIT timekeeping clock.
     */
    uint64_t manager_one_shot_deadline =
        timer_get_deadline(&manager_one_shot);

    while (time_get_ticks() <
           manager_one_shot_deadline)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    /*
     * The expired one-shot must be processed exactly once
     * and must become inactive.
     */
    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_is_expired(&manager_one_shot) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT DEACTIVATE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * A processed one-shot must not fire again.
     */
    if (timer_manager_process() != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT REPEAT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 ONE-SHOT: VERIFIED\n"
    );

    /*
     * Remove the one-shot and verify manager accounting.
     */
    if (timer_manager_remove(&manager_one_shot) != 0 ||
        timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 REMOVE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 REMOVE: VERIFIED\n"
    );

    /*
     * Prepare a periodic timer.
     */
    timer_init(&manager_periodic);

    if (timer_start_periodic(&manager_periodic, 2) != 0)
    {
        serial_write_string(
            "TIMER-2 PERIODIC START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_add(&manager_periodic) != 0 ||
        timer_manager_get_count() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * First periodic cycle.
     */
    uint64_t manager_periodic_deadline_1 =
        timer_get_deadline(&manager_periodic);

    while (time_get_ticks() <
           manager_periodic_deadline_1)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t manager_periodic_deadline_2 =
        timer_get_deadline(&manager_periodic);

    if (manager_periodic_deadline_2 !=
        manager_periodic_deadline_1 + 2)
    {
        serial_write_string(
            "TIMER-2 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 PERIODIC REARM: VERIFIED\n"
    );

    /*
     * Second periodic cycle proves that the manager can
     * continue processing the same registered timer.
     */
    while (time_get_ticks() <
           manager_periodic_deadline_2)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC SECOND EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Cancel the periodic timer and remove it from the
     * manager. No registered timers should remain.
     */
    timer_cancel(&manager_periodic);

    if (timer_manager_remove(&manager_periodic) != 0 ||
        timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 PERIODIC REMOVE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 PERIODIC: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-2 TIMER MANAGER: VERIFIED\n"
    );

    /* --------------------------------------------------------
       TIMER-4 LAPIC CLOCK-SOURCE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "TIMER-4 LAPIC CLOCK VERIFICATION START\n"
    );

    /*
     * The LAPIC timer is already configured by Timer-4 as
     * a periodic 100 Hz clock source.
     *
     * Do not reprogram, stop, or mask it here.
     * This test observes the live clock path.
     */

    uint32_t timer4_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if ((timer4_lvt & 0xFFU) !=
        LAPIC_LVT_TIMER_VECTOR)
    {
        serial_write_string(
            "TIMER-4 LAPIC LVT VECTOR: FAILED\n"
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

    if ((timer4_lvt & LAPIC_LVT_TIMER_PERIODIC) == 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC MODE: NOT PERIODIC\n"
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

    if ((timer4_lvt & LAPIC_LVT_TIMER_MASK) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC MASK: FAILED\n"
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
        "TIMER-4 LAPIC LVT: PERIODIC + UNMASKED\n"
    );

    if (clock_event_get_source() !=
        CLOCK_EVENT_SOURCE_LAPIC ||
        clock_event_get_frequency() != 100)
    {
        serial_write_string(
            "TIMER-4 CLOCK-EVENT STATE: FAILED\n"
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
        "TIMER-4 CLOCK-EVENT SOURCE: LAPIC 100HZ\n"
    );

    uint64_t timer4_interrupts_before =
        lapic_timer_get_interrupt_count();

    uint64_t timer4_clock_events_before =
        clock_event_get_count();

    uint64_t timer4_time_ticks_before =
        time_get_ticks();

    /*
     * Enable interrupts and wait for the live periodic
     * LAPIC timer to deliver at least one clock event.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    while (lapic_timer_get_interrupt_count() ==
           timer4_interrupts_before)
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    /*
     * Stop maskable interrupts while validating the
     * resulting clock/time state.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    uint64_t timer4_interrupts_after =
        lapic_timer_get_interrupt_count();

    uint64_t timer4_clock_events_after =
        clock_event_get_count();

    uint64_t timer4_time_ticks_after =
        time_get_ticks();

    if (timer4_interrupts_after <=
        timer4_interrupts_before)
    {
        serial_write_string(
            "TIMER-4 LAPIC INTERRUPT PROGRESSION: FAILED\n"
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

    if (timer4_clock_events_after <=
        timer4_clock_events_before)
    {
        serial_write_string(
            "TIMER-4 CLOCK-EVENT PROGRESSION: FAILED\n"
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

    if (timer4_time_ticks_after <=
        timer4_time_ticks_before)
    {
        serial_write_string(
            "TIMER-4 TIMEKEEPING PROGRESSION: FAILED\n"
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

    /*
     * Re-read the LVT after real interrupt delivery.
     * The periodic clock source must remain active.
     */
    uint32_t timer4_final_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if ((timer4_final_lvt & 0xFFU) !=
            LAPIC_LVT_TIMER_VECTOR ||
        (timer4_final_lvt &
            LAPIC_LVT_TIMER_PERIODIC) == 0 ||
        (timer4_final_lvt &
            LAPIC_LVT_TIMER_MASK) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC FINAL STATE: FAILED\n"
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
        "TIMER-4 LAPIC INTERRUPT PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 CLOCK-EVENT PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 TIMEKEEPING PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 LAPIC FINAL STATE: PERIODIC + UNMASKED\n"
    );

    serial_write_string(
        "TIMER-4 LAPIC CLOCK SOURCE: VERIFIED\n"
    );
}
