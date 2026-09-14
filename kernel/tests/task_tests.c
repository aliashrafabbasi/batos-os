#include "task_tests.h"

#include "../sched/task.h"
#include "../sched/scheduler.h"
#include "../sched/runqueue.h"
#include "../sched/task_registry.h"
#include "../arch/x86_64/sched/context.h"
#include "../mm/pmm/pmm.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static struct task task_execution_a;
static struct task task_execution_b;

static struct x86_64_context task_execution_harness_context;

static volatile uint64_t task_execution_a_reached = 0;
static volatile uint64_t task_execution_b_reached = 0;
static volatile uint64_t task_execution_a_terminated = 0;

static uint64_t task_execution_expected_argument =
    0x4241544F535F5441ULL;

static void task_execution_test_a(void *argument);
static void task_execution_test_b(void *argument);

extern void x86_64_task_bootstrap_trampoline(void);

static void task_test_entry(void *argument)
{
    (void)argument;
}

static void task_test_fail(const char *message)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

void task_tests_run(void)
{
    serial_write_string(
        "\nTASK FOUNDATION TEST\n"
    );

    struct task task = {0};

    uint64_t pml4 =
        vmm_get_pml4();

    if (pml4 == 0)
    {
        task_test_fail(
            "TASK TEST: NO ADDRESS SPACE\n"
        );
    }

    uint64_t free_before =
        pmm_get_free_frames();

    if (task_create(
            &task,
            1,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK CREATE: FAILED\n"
        );
    }

    uint64_t free_after_create =
        pmm_get_free_frames();

    if (task.state != TASK_STATE_READY ||
        task.kernel_stack_base == 0 ||
        task.kernel_stack_top <=
            task.kernel_stack_base ||
        task.context.rsp !=
            task.kernel_stack_top -
            3ULL * sizeof(uint64_t) ||
        task.context.rip == 0 ||
        free_before - free_after_create !=
            TASK_KERNEL_STACK_PAGE_COUNT)
    {
        task_test_fail(
            "TASK CREATE: INVALID\n"
        );
    }

    /*
     * Verify the initial bootstrap context ABI:
     *
     *   RSP % 16 == 8
     *   [RSP + 0] == 0
     *   [RSP + 8] == task
     *   RIP == bootstrap trampoline
     */
    uint64_t bootstrap_rsp = task.context.rsp;

    if ((bootstrap_rsp & 0xFULL) != 8 ||
        *(uint64_t *)(uintptr_t)bootstrap_rsp != 0 ||
        *(uint64_t *)(uintptr_t)(bootstrap_rsp +
                                 sizeof(uint64_t)) !=
            (uint64_t)(uintptr_t)&task ||
        task.context.rip !=
            (uint64_t)(uintptr_t)x86_64_task_bootstrap_trampoline)
    {
        task_test_fail(
            "TASK BOOTSTRAP STACK: INVALID\n"
        );
    }

    serial_write_string(
        "TASK BOOTSTRAP STACK: VERIFIED\n"
    );

    /*
     * Verify every stack page has a VMM translation.
     */
    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        uint64_t translated = 0;

        uint64_t virtual_address =
            task.kernel_stack_base +
            page * VMM_PAGE_SIZE;

        if (vmm_translate(
                pml4,
                virtual_address,
                &translated
            ) != 0)
        {
            task_test_fail(
                "TASK STACK MAP: FAILED\n"
            );
        }

        if (translated !=
            task.kernel_stack_pages[page])
        {
            task_test_fail(
                "TASK STACK OWNERSHIP: INVALID\n"
            );
        }
    }

    /*
     * The guard page immediately below the stack must remain
     * unmapped.
     */
    uint64_t guard_physical = 0;

    if (vmm_translate(
            pml4,
            task.kernel_stack_base -
                TASK_KERNEL_STACK_GUARD_SIZE,
            &guard_physical
        ) == 0)
    {
        task_test_fail(
            "TASK STACK GUARD: MAPPED\n"
        );
    }

    if (task_destroy(&task) != 0)
    {
        task_test_fail(
            "TASK DESTROY: FAILED\n"
        );
    }

    uint64_t free_after_destroy =
        pmm_get_free_frames();

    if (task.state != TASK_STATE_TERMINATED ||
        free_after_destroy != free_before)
    {
        task_test_fail(
            "TASK STACK RELEASE: FAILED\n"
        );
    }

    serial_write_string(
        "TASK OBJECT: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK MAPPING: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK GUARD: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK OWNERSHIP: VERIFIED\n"
    );

    /*
     * Lifecycle ownership contract:
     *
     * A READY task may be destroyed while it has no external
     * scheduler membership. Once admitted to the runqueue,
     * runnable ownership must be released before task-owned
     * resources are destroyed.
     */
    struct task lifecycle_task = {0};

    if (scheduler_init() != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SCHEDULER INIT: FAILED\n"
        );
    }

    if (task_create(
            &lifecycle_task,
            4,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&lifecycle_task) != 0 ||
        !runqueue_contains(&lifecycle_task))
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE ADMISSION: FAILED\n"
        );
    }

    if (runqueue_remove(&lifecycle_task) != 0 ||
        runqueue_contains(&lifecycle_task))
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE RELEASE: FAILED\n"
        );
    }

    if (task_destroy(&lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE DESTROY: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE RUNQUEUE OWNERSHIP: VERIFIED\n"
    );

    /*
     * Registry membership is independently owned. It must be
     * released before final task resource destruction.
     */
    struct task registry_lifecycle_task = {0};

    if (task_create(
            &registry_lifecycle_task,
            5,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY CREATE: FAILED\n"
        );
    }

    if (task_registry_init() != 0 ||
        task_registry_register(&registry_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY ADMISSION: FAILED\n"
        );
    }

    if (task_registry_find(
            registry_lifecycle_task.id
        ) != &registry_lifecycle_task)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY OWNERSHIP: FAILED\n"
        );
    }

    if (task_registry_unregister(
            &registry_lifecycle_task
        ) != 0 ||
        task_registry_find(
            registry_lifecycle_task.id
        ) != NULL)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY RELEASE: FAILED\n"
        );
    }

    if (task_destroy(&registry_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY DESTROY: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE REGISTRY OWNERSHIP: VERIFIED\n"
    );

    /*
     * Real task execution/termination verification.
     *
     * Task A executes its entry, verifies its argument, exits,
     * and the scheduler transfers control to Task B without
     * re-queueing the terminated task.
     */

    if (task_create(
            &task_execution_a,
            2,
            pml4,
            task_execution_test_a,
            (void *)(uintptr_t)
                task_execution_expected_argument
        ) != 0 ||
        task_create(
            &task_execution_b,
            3,
            pml4,
            task_execution_test_b,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK EXECUTION CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&task_execution_a) != 0 ||
        scheduler_add(&task_execution_b) != 0 ||
        scheduler_start() != 0)
    {
        task_test_fail(
            "TASK EXECUTION ADMISSION: FAILED\n"
        );
    }

    x86_64_context_switch(
        &task_execution_harness_context,
        &task_execution_a.context
    );

    if (task_execution_a_reached != 1 ||
        task_execution_b_reached != 1 ||
        task_execution_a_terminated != 1)
    {
        task_test_fail(
            "TASK EXECUTION: FAILED\n"
        );
    }

    if (task_execution_a.state !=
            TASK_STATE_TERMINATED ||
        task_execution_b.state !=
            TASK_STATE_RUNNING ||
        scheduler_get_current() !=
            &task_execution_b)
    {
        task_test_fail(
            "TASK TERMINATION STATE: FAILED\n"
        );
    }

    if (scheduler_get_dispatch_count() != 1)
    {
        task_test_fail(
            "TASK TERMINATION DISPATCH: FAILED\n"
        );
    }

    if (task_destroy(&task_execution_a) != 0)
    {
        task_test_fail(
            "TASK TERMINATED CLEANUP: FAILED\n"
        );
    }

    /*
     * B is the currently running task. Do not destroy it here.
     * Its stack remains valid until a later non-running cleanup
     * path exists.
     */
    serial_write_string(
        "TASK EXECUTION: VERIFIED\n"
    );

    serial_write_string(
        "TASK ENTRY ARGUMENT: VERIFIED\n"
    );

    serial_write_string(
        "TASK EXIT: VERIFIED\n"
    );

    serial_write_string(
        "TASK TERMINATION DISPATCH: VERIFIED\n"
    );
}

static void task_execution_test_a(void *argument)
{
    task_execution_a_reached = 1;

    if ((uint64_t)(uintptr_t)argument !=
        task_execution_expected_argument)
    {
        task_test_fail(
            "TASK ENTRY ARGUMENT: FAILED\n"
        );
    }

    task_execution_a_terminated = 1;
}

static void task_execution_test_b(void *argument)
{
    (void)argument;

    task_execution_b_reached = 1;

    /*
     * Return control to the test harness without returning into
     * task bootstrap. Task B remains RUNNING because it is the
     * active task after Task A terminates.
     */
    x86_64_context_switch(
        &task_execution_b.context,
        &task_execution_harness_context
    );

    task_test_fail(
        "TASK EXECUTION HARNESS RETURN: FAILED\n"
    );
}
