#include "preempt_runtime_tests.h"

#include "../arch/x86_64/apic/lapic.h"
#include "../arch/x86_64/sched/preempt.h"
#include "../console/console.h"
#include "../mm/vmm/vmm.h"
#include "../sched/scheduler.h"
#include "../sched/task.h"

#include <stdint.h>

static struct task preempt_runtime_task_a;
static struct task preempt_runtime_task_b;

static struct x86_64_context preempt_runtime_harness_context;

static volatile uint64_t preempt_runtime_a_progress;
static volatile uint64_t preempt_runtime_b_progress;

static uint64_t preempt_runtime_timer_baseline;
static uint64_t preempt_runtime_dispatch_baseline;

static void preempt_runtime_halt_failure(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );
    }
}

static void preempt_runtime_halt_success(void)
{
    x86_64_preempt_disable();

    serial_write_string(
        "PREEMPTION-2 TIMER-DRIVEN RUNTIME: VERIFIED\n"
    );

    __asm__ volatile (
        "cli\n"
        "1:\n"
        "hlt\n"
        "jmp 1b\n"
    );

    __builtin_unreachable();
}

static int preempt_runtime_progress_verified(void)
{
    uint64_t timer_delta =
        lapic_timer_get_interrupt_count() -
        preempt_runtime_timer_baseline;

    uint64_t dispatch_delta =
        scheduler_get_dispatch_count() -
        preempt_runtime_dispatch_baseline;

    /*
     * Both tasks must independently execute enough work, and
     * scheduler dispatches must have occurred after the runtime
     * test enabled preemption.
     *
     * With two READY tasks, every successful timer preemption
     * transfers ownership to the other task.
     */
    return
        preempt_runtime_a_progress >= 1000000ULL &&
        preempt_runtime_b_progress >= 1000000ULL &&
        timer_delta >= 2 &&
        dispatch_delta >= 2;
}

static void preempt_runtime_task_a_entry(void *argument)
{
    (void)argument;

    for (;;)
    {
        preempt_runtime_a_progress++;

        if (preempt_runtime_progress_verified())
        {
            serial_write_string(
                "PREEMPTION-2 A/B PROGRESS: VERIFIED\n"
            );

            serial_write_string(
                "PREEMPTION-2 LAPIC TIMER DISPATCH: VERIFIED\n"
            );

            preempt_runtime_halt_success();
        }
    }
}

static void preempt_runtime_task_b_entry(void *argument)
{
    (void)argument;

    for (;;)
    {
        preempt_runtime_b_progress++;

        if (preempt_runtime_progress_verified())
        {
            serial_write_string(
                "PREEMPTION-2 B/A PROGRESS: VERIFIED\n"
            );

            serial_write_string(
                "PREEMPTION-2 LAPIC TIMER DISPATCH: VERIFIED\n"
            );

            preempt_runtime_halt_success();
        }
    }
}

void preempt_runtime_tests_run(void)
{
    uint64_t pml4;

    pml4 = vmm_get_pml4();

    if (pml4 == 0)
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 ADDRESS SPACE: FAILED\n"
        );
    }

    preempt_runtime_a_progress = 0;
    preempt_runtime_b_progress = 0;

    if (x86_64_preempt_is_enabled())
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 INITIAL STATE: FAILED\n"
        );
    }

    if (scheduler_init() != 0)
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 SCHEDULER INIT: FAILED\n"
        );
    }

    if (task_create(
            &preempt_runtime_task_a,
            240,
            pml4,
            preempt_runtime_task_a_entry,
            NULL
        ) != 0 ||
        task_create(
            &preempt_runtime_task_b,
            241,
            pml4,
            preempt_runtime_task_b_entry,
            NULL
        ) != 0)
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 TASK CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&preempt_runtime_task_a) != 0 ||
        scheduler_add(&preempt_runtime_task_b) != 0 ||
        scheduler_start() != 0)
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 SCHEDULER ADMISSION: FAILED\n"
        );
    }

    if (scheduler_get_current() !=
            &preempt_runtime_task_a ||
        preempt_runtime_task_a.state !=
            TASK_STATE_RUNNING ||
        preempt_runtime_task_b.state !=
            TASK_STATE_READY)
    {
        preempt_runtime_halt_failure(
            "PREEMPTION-2 INITIAL OWNERSHIP: FAILED\n"
        );
    }

    /*
     * Capture runtime-local evidence baselines. Earlier timer
     * and scheduler tests must not count toward M8.6 proof.
     */
    preempt_runtime_timer_baseline =
        lapic_timer_get_interrupt_count();

    preempt_runtime_dispatch_baseline =
        scheduler_get_dispatch_count();

    serial_write_string(
        "PREEMPTION-2 TIMER-DRIVEN RUNTIME: START\n"
    );

    /*
     * Software preemption authority is enabled while A owns the
     * CPU. The initial entry into A remains an explicit test
     * harness -> task context transfer.
     *
     * Once A starts running, no task calls scheduler_yield().
     * All subsequent task ownership changes must therefore come
     * through the LAPIC timer preemption boundary.
     */
    x86_64_preempt_enable();

    x86_64_context_switch(
        &preempt_runtime_harness_context,
        &preempt_runtime_task_a.context
    );

    /*
     * A correct runtime proof terminates inside
     * preempt_runtime_halt_success(). Returning here means the
     * initial context transfer unexpectedly returned.
     */
    preempt_runtime_halt_failure(
        "PREEMPTION-2 INITIAL TRANSFER: FAILED\n"
    );
}
