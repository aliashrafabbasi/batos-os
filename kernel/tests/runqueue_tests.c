#include "runqueue_tests.h"

#include "../sched/runqueue.h"
#include "../sched/task.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static void runqueue_test_fail(const char *message)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void runqueue_test_entry(void *argument)
{
    (void)argument;
}

static void prepare_task(
    struct task *task,
    uint64_t id
)
{
    task->id = id;
    task->state = TASK_STATE_READY;
    task->entry = runqueue_test_entry;
    task->argument = NULL;
}

void runqueue_tests_run(void)
{
    serial_write_string(
        "\nRUNQUEUE FOUNDATION TEST\n"
    );

    struct task task_a = {0};
    struct task task_b = {0};
    struct task task_c = {0};

    prepare_task(&task_a, 1);
    prepare_task(&task_b, 2);
    prepare_task(&task_c, 3);

    if (runqueue_init() != 0 ||
        runqueue_count() != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE INIT: FAILED\n"
        );
    }

    if (runqueue_dequeue() != NULL)
    {
        runqueue_test_fail(
            "RUNQUEUE EMPTY DEQUEUE: FAILED\n"
        );
    }

    if (runqueue_enqueue(NULL) == 0 ||
        runqueue_remove(NULL) == 0 ||
        runqueue_contains(NULL) != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE INVALID INPUT: FAILED\n"
        );
    }

    task_a.state = TASK_STATE_RUNNING;

    if (runqueue_enqueue(&task_a) == 0)
    {
        runqueue_test_fail(
            "RUNQUEUE STATE VALIDATION: FAILED\n"
        );
    }

    task_a.state = TASK_STATE_READY;

    if (runqueue_enqueue(&task_a) != 0 ||
        runqueue_enqueue(&task_b) != 0 ||
        runqueue_enqueue(&task_c) != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE ENQUEUE: FAILED\n"
        );
    }

    if (runqueue_count() != 3 ||
        !runqueue_contains(&task_a) ||
        !runqueue_contains(&task_b) ||
        !runqueue_contains(&task_c))
    {
        runqueue_test_fail(
            "RUNQUEUE MEMBERSHIP: FAILED\n"
        );
    }

    if (runqueue_enqueue(&task_a) == 0 ||
        runqueue_count() != 3)
    {
        runqueue_test_fail(
            "RUNQUEUE DUPLICATE: FAILED\n"
        );
    }

    if (runqueue_peek() != &task_a ||
        runqueue_count() != 3 ||
        !runqueue_contains(&task_a) ||
        !runqueue_contains(&task_b) ||
        !runqueue_contains(&task_c))
    {
        runqueue_test_fail(
            "RUNQUEUE PEEK: FAILED\n"
        );
    }

    if (runqueue_dequeue() != &task_a ||
        runqueue_dequeue() != &task_b ||
        runqueue_dequeue() != &task_c ||
        runqueue_count() != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE FIFO: FAILED\n"
        );
    }

    if (runqueue_enqueue(&task_a) != 0 ||
        runqueue_enqueue(&task_b) != 0 ||
        runqueue_enqueue(&task_c) != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE REUSE: FAILED\n"
        );
    }

    if (runqueue_remove(&task_b) != 0 ||
        runqueue_count() != 2 ||
        runqueue_dequeue() != &task_a ||
        runqueue_dequeue() != &task_c)
    {
        runqueue_test_fail(
            "RUNQUEUE MIDDLE REMOVE: FAILED\n"
        );
    }

    if (runqueue_remove(&task_a) == 0 ||
        runqueue_remove(&task_b) == 0 ||
        runqueue_remove(&task_c) == 0)
    {
        runqueue_test_fail(
            "RUNQUEUE ABSENT REMOVE: FAILED\n"
        );
    }

    if (runqueue_enqueue(&task_a) != 0 ||
        runqueue_enqueue(&task_b) != 0 ||
        runqueue_enqueue(&task_c) != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE FINAL REUSE: FAILED\n"
        );
    }

    task_a.state = TASK_STATE_BLOCKED;

    if (runqueue_contains(&task_a) != 1)
    {
        runqueue_test_fail(
            "RUNQUEUE MEMBERSHIP STATE COUPLING: FAILED\n"
        );
    }

    if (runqueue_remove(&task_a) != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE BLOCKED REMOVE: FAILED\n"
        );
    }

    /*
     * Reset the queue so the wrap-around test has deterministic
     * head/tail positions independent of earlier test operations.
     */
    if (runqueue_init() != 0 ||
        runqueue_count() != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE WRAP RESET: FAILED\n"
        );
    }

    /*
     * Advance head and tail to slot 254. Each cycle reuses the
     * same task only after it has been dequeued.
     */
    struct task advance_task;

    prepare_task(&advance_task, 10);

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS - 2;
         index++)
    {
        if (runqueue_enqueue(&advance_task) != 0 ||
            runqueue_dequeue() != &advance_task)
        {
            runqueue_test_fail(
                "RUNQUEUE WRAP ADVANCE: FAILED\n"
            );
        }
    }

    /*
     * These four entries occupy physical slots:
     * 254 -> 255 -> 0 -> 1
     */
    struct task wrap_tasks[4];

    for (uint64_t index = 0; index < 4; index++)
    {
        prepare_task(&wrap_tasks[index], index + 20);
    }

    if (runqueue_enqueue(&wrap_tasks[0]) != 0 ||
        runqueue_enqueue(&wrap_tasks[1]) != 0 ||
        runqueue_enqueue(&wrap_tasks[2]) != 0 ||
        runqueue_enqueue(&wrap_tasks[3]) != 0 ||
        runqueue_count() != 4)
    {
        runqueue_test_fail(
            "RUNQUEUE WRAP ENQUEUE: FAILED\n"
        );
    }

    /*
     * Remove an entry from the wrapped portion. This validates
     * compaction across the physical end of the ring.
     */
    if (runqueue_remove(&wrap_tasks[1]) != 0 ||
        runqueue_count() != 3)
    {
        runqueue_test_fail(
            "RUNQUEUE WRAPPED REMOVE: FAILED\n"
        );
    }

    if (runqueue_dequeue() != &wrap_tasks[0] ||
        runqueue_dequeue() != &wrap_tasks[2] ||
        runqueue_dequeue() != &wrap_tasks[3] ||
        runqueue_count() != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE WRAP FIFO: FAILED\n"
        );
    }

    /*
     * Fill the queue to its complete bounded capacity after the
     * ring has already crossed its physical boundary.
     */
    struct task capacity_tasks[TASK_MAX_TASKS];

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        prepare_task(
            &capacity_tasks[index],
            index + 1
        );

        if (runqueue_enqueue(&capacity_tasks[index]) != 0)
        {
            runqueue_test_fail(
                "RUNQUEUE CAPACITY FILL: FAILED\n"
            );
        }
    }

    if (runqueue_count() != TASK_MAX_TASKS)
    {
        runqueue_test_fail(
            "RUNQUEUE CAPACITY COUNT: FAILED\n"
        );
    }

    /*
     * The queue must reject an additional task without changing
     * its count or existing FIFO contents.
     */
    task_a.state = TASK_STATE_READY;

    if (runqueue_enqueue(&task_a) == 0 ||
        runqueue_count() != TASK_MAX_TASKS)
    {
        runqueue_test_fail(
            "RUNQUEUE CAPACITY OVERFLOW: FAILED\n"
        );
    }

    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        if (runqueue_dequeue() != &capacity_tasks[index])
        {
            runqueue_test_fail(
                "RUNQUEUE CAPACITY FIFO: FAILED\n"
            );
        }
    }

    if (runqueue_count() != 0)
    {
        runqueue_test_fail(
            "RUNQUEUE CAPACITY DRAIN: FAILED\n"
        );
    }

    serial_write_string(
        "RUNQUEUE INIT: VERIFIED\n"
    );

    serial_write_string(
        "RUNQUEUE ENQUEUE: VERIFIED\n"
    );

    serial_write_string(
        "RUNQUEUE FIFO: VERIFIED\n"
    );

    serial_write_string(
        "RUNQUEUE DUPLICATE PROTECTION: VERIFIED\n"
    );

    serial_write_string(
        "RUNQUEUE REMOVE: VERIFIED\n"
    );

    serial_write_string(
        "RUNQUEUE STATE INDEPENDENCE: VERIFIED\n"
    );
}
