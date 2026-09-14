#include "scheduler_tests.h"

#include "../sched/scheduler.h"
#include "../sched/task.h"
#include "../sched/runqueue.h"
#include "../console/console.h"
#include "../arch/x86_64/sched/context.h"

#include <stdint.h>
#include <stddef.h>

static struct task scheduler_task_a;
static struct task scheduler_task_b;

static struct x86_64_context scheduler_test_harness_context;

static uint8_t scheduler_stack_a[4096]
    __attribute__((aligned(16)));

static uint8_t scheduler_stack_b[4096]
    __attribute__((aligned(16)));

static volatile uint64_t scheduler_a_reached = 0;
static volatile uint64_t scheduler_b_reached = 0;
static volatile uint64_t scheduler_a_resumed = 0;

static void scheduler_test_fail(const char *message)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void scheduler_test_a(void *argument);
static void scheduler_test_b(void *argument);

static void scheduler_prepare_context(
    struct task *task,
    uint64_t id,
    uint8_t *stack,
    task_entry_t entry
)
{
    uint64_t stack_top =
        (uint64_t)(uintptr_t)(stack + 4096);

    stack_top -= sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)stack_top =
        (uint64_t)(uintptr_t)entry;

    task->id = id;
    task->state = TASK_STATE_READY;
    task->entry = entry;
    task->argument = NULL;

    task->context.rbx = 0;
    task->context.rbp = 0;
    task->context.r12 = 0;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;
    task->context.rsp = stack_top;
    task->context.rip = (uint64_t)(uintptr_t)entry;
}

static void scheduler_test_a(void *argument)
{
    (void)argument;

    scheduler_a_reached = 1;

    if (scheduler_get_current() != &scheduler_task_a)
    {
        scheduler_test_fail(
            "SCHEDULER CURRENT A: FAILED\n"
        );
    }

    if (scheduler_yield() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER A YIELD: FAILED\n"
        );
    }

    scheduler_a_resumed = 1;

    x86_64_context_switch(
        &scheduler_task_a.context,
        &scheduler_test_harness_context
    );

    scheduler_test_fail(
        "SCHEDULER A HARNESS RETURN: FAILED\n"
    );
}

static void scheduler_test_b(void *argument)
{
    (void)argument;

    scheduler_b_reached = 1;

    if (scheduler_get_current() != &scheduler_task_b)
    {
        scheduler_test_fail(
            "SCHEDULER CURRENT B: FAILED\n"
        );
    }

    if (scheduler_yield() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER B YIELD: FAILED\n"
        );
    }

    scheduler_test_fail(
        "SCHEDULER B RESUMED UNEXPECTEDLY\n"
    );
}

void scheduler_tests_run(void)
{
    serial_write_string(
        "\nSCHEDULER FOUNDATION TEST\n"
    );

    scheduler_prepare_context(
        &scheduler_task_a,
        1,
        scheduler_stack_a,
        scheduler_test_a
    );

    scheduler_prepare_context(
        &scheduler_task_b,
        2,
        scheduler_stack_b,
        scheduler_test_b
    );

    if (scheduler_init() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER INIT: FAILED\n"
        );
    }

    if (scheduler_get_current() != NULL ||
        scheduler_get_dispatch_count() != 0 ||
        runqueue_count() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER INITIAL STATE: FAILED\n"
        );
    }

    /*
     * Scheduler rejection-path contract.
     *
     * Invalid scheduler operations must fail without creating
     * scheduler ownership or changing the initial state.
     */
    if (scheduler_start() == 0 ||
        scheduler_get_current() != NULL ||
        scheduler_get_dispatch_count() != 0 ||
        runqueue_count() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER EMPTY START ATOMICITY: FAILED\n"
        );
    }

    if (scheduler_yield() == 0 ||
        scheduler_get_current() != NULL ||
        scheduler_get_dispatch_count() != 0 ||
        runqueue_count() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER EMPTY YIELD: FAILED\n"
        );
    }

    if (scheduler_add(NULL) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER NULL ADMISSION: FAILED\n"
        );
    }

    struct task invalid_task = {0};

    invalid_task.state = TASK_STATE_NEW;
    if (scheduler_add(&invalid_task) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER NEW ADMISSION: FAILED\n"
        );
    }

    invalid_task.state = TASK_STATE_BLOCKED;
    if (scheduler_add(&invalid_task) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER BLOCKED ADMISSION: FAILED\n"
        );
    }

    invalid_task.state = TASK_STATE_SLEEPING;
    if (scheduler_add(&invalid_task) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER SLEEPING ADMISSION: FAILED\n"
        );
    }

    invalid_task.state = TASK_STATE_RUNNING;
    if (scheduler_add(&invalid_task) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER RUNNING ADMISSION: FAILED\n"
        );
    }

    invalid_task.state = TASK_STATE_TERMINATED;
    if (scheduler_add(&invalid_task) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER TERMINATED ADMISSION: FAILED\n"
        );
    }

    if (scheduler_add(&scheduler_task_a) != 0 ||
        scheduler_add(&scheduler_task_b) != 0)
    {
        scheduler_test_fail(
            "SCHEDULER READY ADMISSION: FAILED\n"
        );
    }

    if (scheduler_add(&scheduler_task_a) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER DUPLICATE ADMISSION: FAILED\n"
        );
    }

    if (runqueue_count() != 2 ||
        !runqueue_contains(&scheduler_task_a) ||
        !runqueue_contains(&scheduler_task_b))
    {
        scheduler_test_fail(
            "SCHEDULER READY OWNERSHIP: FAILED\n"
        );
    }
    if (scheduler_start() != 0)
    {
        scheduler_test_fail(
            "SCHEDULER START: FAILED\n"
        );
    }

    if (scheduler_start() == 0)
    {
        scheduler_test_fail(
            "SCHEDULER SECOND START: FAILED\n"
        );
    }

    if (scheduler_get_current() != &scheduler_task_a ||
        scheduler_task_a.state != TASK_STATE_RUNNING ||
        runqueue_contains(&scheduler_task_a) ||
        scheduler_task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&scheduler_task_b) ||
        runqueue_count() != 1)
    {
        scheduler_test_fail(
            "SCHEDULER CURRENT TASK: FAILED\n"
        );
    }

    /*
     * Test harness -> Task A.
     *
     * scheduler_start() establishes A as the current task but does
     * not perform the initial context transfer. This explicit
     * test-only transfer enters A using its real saved task context.
     */
    x86_64_context_switch(
        &scheduler_test_harness_context,
        &scheduler_task_a.context
    );

    /*
     * A is RUNNING and B is READY at this point.
     *
     * Only the current task may request termination dispatch, and
     * the current task must already be TERMINATED before the
     * scheduler accepts that handoff.
     */
    if (scheduler_exit_current(NULL) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER NULL EXIT: FAILED\n"
        );
    }

    if (scheduler_exit_current(&scheduler_task_b) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER NONCURRENT EXIT: FAILED\n"
        );
    }

    if (scheduler_exit_current(&scheduler_task_a) == 0)
    {
        scheduler_test_fail(
            "SCHEDULER RUNNING EXIT: FAILED\n"
        );
    }

    if (scheduler_get_current() != &scheduler_task_a ||
        scheduler_task_a.state != TASK_STATE_RUNNING ||
        runqueue_contains(&scheduler_task_a) ||
        scheduler_task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&scheduler_task_b) ||
        runqueue_count() != 1 ||
        scheduler_get_dispatch_count() != 2)
    {
        scheduler_test_fail(
            "SCHEDULER EXIT REJECTION STATE: FAILED\n"
        );
    }

    if (scheduler_a_reached != 1 ||
        scheduler_b_reached != 1 ||
        scheduler_a_resumed != 1)
    {
        scheduler_test_fail(
            "SCHEDULER COOPERATIVE SWITCH: FAILED\n"
        );
    }

    if (scheduler_get_current() != &scheduler_task_a ||
        scheduler_task_a.state != TASK_STATE_RUNNING ||
        runqueue_contains(&scheduler_task_a) ||
        scheduler_task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&scheduler_task_b) ||
        runqueue_count() != 1)
    {
        scheduler_test_fail(
            "SCHEDULER FINAL OWNERSHIP: FAILED\n"
        );
    }

    if (scheduler_get_dispatch_count() != 2)
    {
        scheduler_test_fail(
            "SCHEDULER DISPATCH COUNT: FAILED\n"
        );
    }

    serial_write_string(
        "SCHEDULER INIT: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER TRANSITION ATOMICITY: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER CURRENT TASK: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER SELECTION: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER STATE REJECTIONS: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER OWNERSHIP INVARIANTS: VERIFIED\n"
    );

    serial_write_string(
        "SCHEDULER COOPERATIVE DISPATCH: VERIFIED\n"
    );
}
