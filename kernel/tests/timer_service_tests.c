#include <stdint.h>

#include "kernel/arch/x86_64/time/time.h"
#include "kernel/arch/x86_64/time/timer.h"
#include "kernel/arch/x86_64/time/timer_manager.h"
#include "kernel/arch/x86_64/time/timer_service.h"
#include "kernel/console/console.h"
#include "kernel/tests/timer_service_tests.h"

static struct timer timer_service_test_timer;

static uint64_t timer_service_test_start_ticks = 0;

void timer_service_tests_run(void)
{
    serial_write_string(
        "TIMEOUT-1 TEST START\n"
    );

    /*
     * Runtime service initialization owns timer-manager
     * initialization. The test timer itself is armed only
     * at the explicit live-runtime boundary.
     */
    timer_service_init();

    if (timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMEOUT-1 INIT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_service_get_processed_count() != 0)
    {
        serial_write_string(
            "TIMEOUT-1 INITIAL SERVICE COUNT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMEOUT-1 INIT: VERIFIED\n"
    );
}

void timer_service_runtime_test_arm(void)
{
    timer_init(
        &timer_service_test_timer
    );

    timer_service_test_start_ticks =
        time_get_ticks();

    if (timer_start(
            &timer_service_test_timer,
            5
        ) != 0)
    {
        serial_write_string(
            "TIMEOUT-1 TIMER START: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_add(
            &timer_service_test_timer
        ) != 0)
    {
        serial_write_string(
            "TIMEOUT-1 TIMER REGISTER: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_get_count() != 1)
    {
        serial_write_string(
            "TIMEOUT-1 TIMER REGISTER: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_get_deadline(
            &timer_service_test_timer
        ) <= timer_service_test_start_ticks)
    {
        serial_write_string(
            "TIMEOUT-1 DEADLINE: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_service_get_processed_count() != 0)
    {
        serial_write_string(
            "TIMEOUT-1 INITIAL SERVICE COUNT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMEOUT-1 RUNTIME TIMER: ARMED\n"
    );
}

void timer_service_runtime_test_run(void)
{
    uint64_t processed_before =
        timer_service_get_processed_count();

    uint64_t deadline =
        timer_get_deadline(
            &timer_service_test_timer
        );

    serial_write_string(
        "TIMEOUT-1 RUNTIME: WAITING\n"
    );

    serial_write_string(
        "TIMEOUT-1 SERVICE COUNT: "
    );
    serial_write_hex(processed_before);
    serial_write_string("\n");

    /*
     * Do not call timer_service_tick() or
     * timer_manager_process() here.
     *
     * Expiration must be produced exclusively by
     * the live LAPIC -> clock-event runtime path.
     */
    while (
        timer_service_get_processed_count() ==
        processed_before
    )
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    uint64_t now =
        time_get_ticks();

    if (now < deadline)
    {
        serial_write_string(
            "TIMEOUT-1 RUNTIME DEADLINE: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_is_expired(
            &timer_service_test_timer
        ) != 0)
    {
        serial_write_string(
            "TIMEOUT-1 ONE-SHOT CONSUMPTION: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_service_get_processed_count() <=
        processed_before)
    {
        serial_write_string(
            "TIMEOUT-1 SERVICE PROCESSING: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * The manager keeps the timer registered after a
     * one-shot timer expires. Explicit removal remains
     * the owner's responsibility.
     */
    if (timer_manager_remove(
            &timer_service_test_timer
        ) != 0)
    {
        serial_write_string(
            "TIMEOUT-1 TIMER REMOVE: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMEOUT-1 FINAL COUNT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMEOUT-1 RUNTIME EXPIRATION: VERIFIED\n"
    );

    serial_write_string(
        "TIMEOUT-1 TIMER SERVICE: VERIFIED\n"
    );
}
