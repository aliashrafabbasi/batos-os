#include "task_registry_tests.h"

#include "../sched/task.h"
#include "../sched/task_registry.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static void task_registry_test_fail(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void task_registry_test_entry(void *argument)
{
    (void)argument;
}

void task_registry_tests_run(void)
{
    serial_write_string(
        "\nTASK REGISTRY FOUNDATION TEST\n"
    );

    if (task_registry_init() != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY INIT: FAILED\n"
        );
    }

    if (task_registry_count() != 0 ||
        task_registry_find(1) != NULL)
    {
        task_registry_test_fail(
            "TASK REGISTRY INIT: INVALID\n"
        );
    }

    struct task task = {0};

    if (task_create(
            &task,
            1,
            0,
            task_registry_test_entry,
            NULL
        ) == 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY TEST: INVALID TASK ACCEPTED\n"
        );
    }

    /*
     * Registry membership is tested independently from task
     * stack ownership. A valid READY task is sufficient for
     * registry identity/lifecycle testing.
     */
    task.id = 1;
    task.state = TASK_STATE_READY;

    if (task_registry_register(&task) != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY REGISTER: FAILED\n"
        );
    }

    if (task_registry_count() != 1 ||
        task_registry_find(1) != &task)
    {
        task_registry_test_fail(
            "TASK REGISTRY LOOKUP: FAILED\n"
        );
    }

    struct task duplicate = {0};

    duplicate.id = 1;
    duplicate.state = TASK_STATE_READY;

    if (task_registry_register(&duplicate) == 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY DUPLICATE: ACCEPTED\n"
        );
    }

    if (task_registry_count() != 1 ||
        task_registry_find(1) != &task)
    {
        task_registry_test_fail(
            "TASK REGISTRY DUPLICATE: CORRUPTED\n"
        );
    }

    if (task_registry_unregister(&duplicate) == 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY WRONG OWNER: ACCEPTED\n"
        );
    }

    if (task_registry_unregister(&task) != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY UNREGISTER: FAILED\n"
        );
    }

    if (task_registry_count() != 0 ||
        task_registry_find(1) != NULL)
    {
        task_registry_test_fail(
            "TASK REGISTRY RELEASE: FAILED\n"
        );
    }

    /*
     * ID reuse must be possible after unregistering the
     * previous owner.
     */
    if (task_registry_register(&duplicate) != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY ID REUSE: FAILED\n"
        );
    }

    if (task_registry_find(1) != &duplicate ||
        task_registry_count() != 1)
    {
        task_registry_test_fail(
            "TASK REGISTRY ID REUSE: INVALID\n"
        );
    }

    if (task_registry_unregister(&duplicate) != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY ID REUSE CLEANUP: FAILED\n"
        );
    }

    /*
     * Fill the complete bounded registry without allocating
     * task stacks. This isolates registry capacity from task
     * resource allocation.
     */
    struct task tasks[TASK_MAX_TASKS];

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        tasks[index].id = index + 1;
        tasks[index].state = TASK_STATE_READY;

        if (task_registry_register(&tasks[index]) != 0)
        {
            task_registry_test_fail(
                "TASK REGISTRY CAPACITY: FAILED\n"
            );
        }
    }

    if (task_registry_count() != TASK_MAX_TASKS)
    {
        task_registry_test_fail(
            "TASK REGISTRY CAPACITY COUNT: INVALID\n"
        );
    }

    struct task overflow = {0};

    overflow.id = TASK_MAX_TASKS;
    overflow.state = TASK_STATE_READY;

    if (task_registry_register(&overflow) == 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY CAPACITY: OVERFLOW ACCEPTED\n"
        );
    }

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        if (task_registry_find(index + 1) !=
            &tasks[index])
        {
            task_registry_test_fail(
                "TASK REGISTRY CAPACITY LOOKUP: FAILED\n"
            );
        }
    }

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        if (task_registry_unregister(&tasks[index]) != 0)
        {
            task_registry_test_fail(
                "TASK REGISTRY CAPACITY CLEANUP: FAILED\n"
            );
        }
    }

    if (task_registry_count() != 0)
    {
        task_registry_test_fail(
            "TASK REGISTRY FINAL COUNT: INVALID\n"
        );
    }

    serial_write_string(
        "TASK REGISTRY INIT: VERIFIED\n"
        "TASK REGISTRY REGISTER: VERIFIED\n"
        "TASK REGISTRY LOOKUP: VERIFIED\n"
        "TASK REGISTRY DUPLICATE PROTECTION: VERIFIED\n"
        "TASK REGISTRY UNREGISTER: VERIFIED\n"
        "TASK REGISTRY ID REUSE: VERIFIED\n"
        "TASK REGISTRY CAPACITY: VERIFIED\n"
    );
}
