#include "runqueue.h"

#include "task.h"

#include <stddef.h>

static struct task *runqueue_tasks[TASK_MAX_TASKS];

static uint64_t runqueue_head;
static uint64_t runqueue_tail;
static uint64_t runqueue_active_count;
static int runqueue_initialized;

int runqueue_init(void)
{
    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        runqueue_tasks[index] = NULL;
    }

    runqueue_head = 0;
    runqueue_tail = 0;
    runqueue_active_count = 0;
    runqueue_initialized = 1;

    return 0;
}

int runqueue_contains(
    const struct task *task
)
{
    if (!runqueue_initialized ||
        task == NULL)
    {
        return 0;
    }

    for (uint64_t index = 0;
         index < runqueue_active_count;
         index++)
    {
        uint64_t position =
            (runqueue_head + index) % TASK_MAX_TASKS;

        if (runqueue_tasks[position] == task)
            return 1;
    }

    return 0;
}

int runqueue_enqueue(
    struct task *task
)
{
    if (!runqueue_initialized ||
        task == NULL)
    {
        return -1;
    }

    if (task->state != TASK_STATE_READY)
        return -1;

    if (runqueue_active_count >= TASK_MAX_TASKS)
        return -1;

    if (runqueue_contains(task))
        return -1;

    runqueue_tasks[runqueue_tail] = task;

    runqueue_tail =
        (runqueue_tail + 1) % TASK_MAX_TASKS;

    runqueue_active_count++;

    return 0;
}

struct task *runqueue_peek(void)
{
    if (!runqueue_initialized ||
        runqueue_active_count == 0)
    {
        return NULL;
    }

    return runqueue_tasks[runqueue_head];
}

struct task *runqueue_dequeue(void)
{
    if (!runqueue_initialized ||
        runqueue_active_count == 0)
    {
        return NULL;
    }

    struct task *task =
        runqueue_tasks[runqueue_head];

    runqueue_tasks[runqueue_head] = NULL;

    runqueue_head =
        (runqueue_head + 1) % TASK_MAX_TASKS;

    runqueue_active_count--;

    return task;
}

int runqueue_remove(
    struct task *task
)
{
    if (!runqueue_initialized ||
        task == NULL ||
        runqueue_active_count == 0)
    {
        return -1;
    }

    for (uint64_t index = 0;
         index < runqueue_active_count;
         index++)
    {
        uint64_t position =
            (runqueue_head + index) % TASK_MAX_TASKS;

        if (runqueue_tasks[position] != task)
            continue;

        /*
         * Compact the FIFO sequence so ordering of all
         * remaining runnable tasks is preserved.
         */
        for (uint64_t shift = index;
             shift + 1 < runqueue_active_count;
             shift++)
        {
            uint64_t current =
                (runqueue_head + shift) %
                TASK_MAX_TASKS;

            uint64_t next =
                (runqueue_head + shift + 1) %
                TASK_MAX_TASKS;

            runqueue_tasks[current] =
                runqueue_tasks[next];
        }

        runqueue_tail =
            (runqueue_tail +
             TASK_MAX_TASKS - 1) %
            TASK_MAX_TASKS;

        runqueue_tasks[runqueue_tail] = NULL;
        runqueue_active_count--;

        return 0;
    }

    return -1;
}

uint64_t runqueue_count(void)
{
    if (!runqueue_initialized)
        return 0;

    return runqueue_active_count;
}
