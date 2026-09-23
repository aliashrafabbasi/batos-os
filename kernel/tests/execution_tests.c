#include "execution_tests.h"

#include "../execution/execution.h"
#include "../mm/vmm/vmm.h"
#include "../process/process_registry.h"
#include "../sched/scheduler.h"
#include "../sched/task_registry.h"
#include "../sched/runqueue.h"
#include "../sched/task.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static void execution_test_fail(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void execution_test_entry(
    void *argument
)
{
    (void)argument;
}

static void execution_cleanup_success(
    struct process *process,
    struct task *task
)
{
    /*
     * Successful Execution creation publishes READY ownership
     * into the scheduler. Remove that ownership before the lower
     * lifecycle destructors are invoked.
     */
    if (runqueue_remove(task) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: RUNQUEUE REMOVE FAILED\n"
        );
    }

    if (process_detach_task(
            process,
            task
        ) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: PROCESS DETACH FAILED\n"
        );
    }

    if (task_registry_unregister(task) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: TASK UNREGISTER FAILED\n"
        );
    }

    if (task_destroy(task) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: TASK DESTROY FAILED\n"
        );
    }

    if (process_terminate(process) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: PROCESS TERMINATE FAILED\n"
        );
    }

    if (process_destroy(process) != 0)
    {
        execution_test_fail(
            "EXECUTION CLEANUP: PROCESS DESTROY FAILED\n"
        );
    }
}

void execution_tests_run(void)
{
    serial_write_string(
        "\nEXECUTION FOUNDATION TEST\n"
    );

    /*
     * Establish an isolated lifecycle publication boundary.
     *
     * Execution is a coordinator over these registries; the test
     * therefore explicitly initializes the registries it exercises.
     */
    if (task_registry_init() != 0 ||
        process_registry_init() != 0 ||
        scheduler_init() != 0)
    {
        execution_test_fail(
            "EXECUTION INIT: FAILED\n"
        );
    }

    if (task_registry_count() != 0 ||
        process_registry_count() != 0 ||
        runqueue_count() != 0 ||
        scheduler_get_current() != NULL)
    {
        execution_test_fail(
            "EXECUTION INIT: INVALID STATE\n"
        );
    }

    uint64_t kernel_address_space =
        vmm_get_pml4();

    if (kernel_address_space == 0)
    {
        execution_test_fail(
            "EXECUTION TEST: KERNEL ADDRESS SPACE INVALID\n"
        );
    }

    /*
     * Successful creation publishes the complete execution unit
     * but does not start the scheduler or transfer CPU execution.
     */
    struct process process = {0};
    struct task task = {0};

    if (execution_create_kernel_task(
            &process,
            &task,
            1,
            1,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) != 0)
    {
        execution_test_fail(
            "EXECUTION CREATE: SUCCESS PATH FAILED\n"
        );
    }

    if (process_registry_count() != 1 ||
        !process_registry_contains(&process) ||
        process.state != PROCESS_STATE_ACTIVE ||
        process.id != 1 ||
        process.address_space != kernel_address_space)
    {
        execution_test_fail(
            "EXECUTION SUCCESS: PROCESS OWNERSHIP FAILED\n"
        );
    }

    if (task_registry_count() != 1 ||
        !task_registry_contains(&task) ||
        task_registry_find(1) != &task)
    {
        execution_test_fail(
            "EXECUTION SUCCESS: TASK REGISTRY FAILED\n"
        );
    }

    if (task.state != TASK_STATE_READY ||
        task.process != &process ||
        task.address_space != kernel_address_space ||
        process.task_count != 1 ||
        process.tasks[0] != &task)
    {
        execution_test_fail(
            "EXECUTION SUCCESS: PROCESS TASK MEMBERSHIP FAILED\n"
        );
    }

    if (!runqueue_contains(&task) ||
        runqueue_count() != 1)
    {
        execution_test_fail(
            "EXECUTION SUCCESS: RUNQUEUE OWNERSHIP FAILED\n"
        );
    }

    /*
     * Creation is publication only. It must not implicitly start
     * or dispatch the scheduler.
     */
    if (scheduler_get_current() != NULL ||
        scheduler_get_dispatch_count() != 0)
    {
        execution_test_fail(
            "EXECUTION SUCCESS: SCHEDULER STARTED IMPLICITLY\n"
        );
    }

    serial_write_string(
        "EXECUTION SUCCESS PUBLICATION: VERIFIED\n"
    );

    execution_cleanup_success(
        &process,
        &task
    );

    if (task_registry_count() != 0 ||
        process_registry_count() != 0 ||
        runqueue_count() != 0)
    {
        execution_test_fail(
            "EXECUTION SUCCESS CLEANUP: FAILED\n"
        );
    }

    serial_write_string(
        "EXECUTION SUCCESS CLEANUP: VERIFIED\n"
    );

    /*
     * Invalid address-space input must be rejected before Process
     * creation. Use a real VMM-created address space to distinguish
     * this from the generic zero-address rejection.
     */
    uint64_t wrong_address_space =
        vmm_create_address_space();

    if (wrong_address_space == 0)
    {
        execution_test_fail(
            "EXECUTION WRONG AS: ADDRESS SPACE CREATE FAILED\n"
        );
    }

    struct process invalid_process = {0};
    struct task invalid_task = {0};

    if (execution_create_kernel_task(
            &invalid_process,
            &invalid_task,
            2,
            2,
            wrong_address_space,
            execution_test_entry,
            NULL
        ) == 0)
    {
        execution_test_fail(
            "EXECUTION WRONG AS: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 0 ||
        task_registry_count() != 0 ||
        runqueue_count() != 0 ||
        process_registry_contains(&invalid_process) ||
        task_registry_contains(&invalid_task))
    {
        execution_test_fail(
            "EXECUTION WRONG AS: PARTIAL OWNERSHIP\n"
        );
    }

    if (vmm_destroy_address_space(
            wrong_address_space
        ) != 0)
    {
        execution_test_fail(
            "EXECUTION WRONG AS: ADDRESS SPACE CLEANUP FAILED\n"
        );
    }

    serial_write_string(
        "EXECUTION ADDRESS-SPACE REJECTION: VERIFIED\n"
    );

    /*
     * Duplicate PID fails during Process creation, before any Task
     * object is published.
     */
    struct process pid_owner = {0};
    struct task pid_owner_task = {0};

    if (execution_create_kernel_task(
            &pid_owner,
            &pid_owner_task,
            3,
            3,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) != 0)
    {
        execution_test_fail(
            "EXECUTION DUPLICATE PID: INITIAL CREATE FAILED\n"
        );
    }

    struct process duplicate_pid_process = {0};
    struct task duplicate_pid_task = {0};

    if (execution_create_kernel_task(
            &duplicate_pid_process,
            &duplicate_pid_task,
            3,
            4,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) == 0)
    {
        execution_test_fail(
            "EXECUTION DUPLICATE PID: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 1 ||
        task_registry_count() != 1 ||
        runqueue_count() != 1 ||
        process_registry_find(3) != &pid_owner ||
        task_registry_find(3) != &pid_owner_task ||
        process_registry_contains(&duplicate_pid_process) ||
        task_registry_contains(&duplicate_pid_task))
    {
        execution_test_fail(
            "EXECUTION DUPLICATE PID: STATE CORRUPTED\n"
        );
    }

    execution_cleanup_success(
        &pid_owner,
        &pid_owner_task
    );

    serial_write_string(
        "EXECUTION PROCESS COLLISION ROLLBACK: VERIFIED\n"
    );

    /*
     * Duplicate TID reaches Task Registry publication and therefore
     * exercises Execution's rollback transaction after task creation.
     */
    struct process tid_owner_process = {0};
    struct task tid_owner_task = {0};

    if (execution_create_kernel_task(
            &tid_owner_process,
            &tid_owner_task,
            4,
            4,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) != 0)
    {
        execution_test_fail(
            "EXECUTION DUPLICATE TID: INITIAL CREATE FAILED\n"
        );
    }

    struct process duplicate_tid_process = {0};
    struct task duplicate_tid_task = {0};

    if (execution_create_kernel_task(
            &duplicate_tid_process,
            &duplicate_tid_task,
            5,
            4,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) == 0)
    {
        execution_test_fail(
            "EXECUTION DUPLICATE TID: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 1 ||
        task_registry_count() != 1 ||
        runqueue_count() != 1 ||
        process_registry_find(5) != NULL ||
        task_registry_find(4) != &tid_owner_task ||
        process_registry_contains(&duplicate_tid_process) ||
        task_registry_contains(&duplicate_tid_task))
    {
        execution_test_fail(
            "EXECUTION DUPLICATE TID: ROLLBACK FAILED\n"
        );
    }

    execution_cleanup_success(
        &tid_owner_process,
        &tid_owner_task
    );

    serial_write_string(
        "EXECUTION TASK REGISTRY ROLLBACK: VERIFIED\n"
    );

    /*
     * Fill the scheduler's READY queue with stackless test tasks.
     * Execution will still create a real Task, but scheduler_add()
     * must reject publication at capacity. Because scheduler_add()
     * is atomic, Execution's rollback must reclaim the Process,
     * Task Registry, Task/Process membership, and Task resources.
     */
    struct task capacity_tasks[TASK_MAX_TASKS];

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        capacity_tasks[index].id = 32 + index;
        capacity_tasks[index].state = TASK_STATE_READY;

        if (runqueue_enqueue(
                &capacity_tasks[index]
            ) != 0)
        {
            execution_test_fail(
                "EXECUTION SCHEDULER CAPACITY: SETUP FAILED\n"
            );
        }
    }

    if (runqueue_count() != TASK_MAX_TASKS)
    {
        execution_test_fail(
            "EXECUTION SCHEDULER CAPACITY: FILL FAILED\n"
        );
    }

    struct process capacity_process = {0};
    struct task capacity_task = {0};

    if (execution_create_kernel_task(
            &capacity_process,
            &capacity_task,
            6,
            6,
            kernel_address_space,
            execution_test_entry,
            NULL
        ) == 0)
    {
        execution_test_fail(
            "EXECUTION SCHEDULER CAPACITY: ACCEPTED\n"
        );
    }

    /*
     * scheduler_add() must have rejected before taking runqueue
     * ownership, allowing the Execution rollback to destroy Task.
     */
    if (process_registry_count() != 0 ||
        task_registry_count() != 0 ||
        runqueue_count() != TASK_MAX_TASKS ||
        process_registry_contains(&capacity_process) ||
        task_registry_contains(&capacity_task) ||
        capacity_task.state != TASK_STATE_TERMINATED ||
        capacity_task.process != NULL)
    {
        execution_test_fail(
            "EXECUTION SCHEDULER CAPACITY: ROLLBACK FAILED\n"
        );
    }

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        if (runqueue_dequeue() != &capacity_tasks[index])
        {
            execution_test_fail(
                "EXECUTION SCHEDULER CAPACITY: DRAIN FAILED\n"
            );
        }
    }

    if (runqueue_count() != 0)
    {
        execution_test_fail(
            "EXECUTION SCHEDULER CAPACITY: DRAIN STATE FAILED\n"
        );
    }

    serial_write_string(
        "EXECUTION SCHEDULER ROLLBACK: VERIFIED\n"
    );

    serial_write_string(
        "EXECUTION TRANSACTION: VERIFIED\n"
    );
}
