#include "wait_queue.h"

#include "task.h"

#include <stddef.h>

int wait_queue_init(
    struct wait_queue *queue
)
{
    if (queue == NULL)
        return -1;

    for (uint64_t index = 0;
         index < WAIT_QUEUE_MAX_TASKS;
         index++)
    {
        queue->tasks[index] = NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->initialized = 1;

    return 0;
}

int wait_queue_contains(
    const struct wait_queue *queue,
    const struct task *task
)
{
    if (queue == NULL ||
        task == NULL ||
        !queue->initialized)
    {
        return 0;
    }

    for (uint64_t index = 0;
         index < queue->count;
         index++)
    {
        uint64_t position =
            (queue->head + index) %
            WAIT_QUEUE_MAX_TASKS;

        if (queue->tasks[position] == task)
            return 1;
    }

    return 0;
}

int wait_queue_enqueue(
    struct wait_queue *queue,
    struct task *task
)
{
    if (queue == NULL ||
        task == NULL ||
        !queue->initialized)
    {
        return -1;
    }

    if (task->state != TASK_STATE_BLOCKED)
        return -1;

    if (queue->count >= WAIT_QUEUE_MAX_TASKS)
        return -1;

    if (wait_queue_contains(queue, task))
        return -1;

    /*
     * A task may have exactly one wait-queue owner.
     */
    if (task->wait_queue != NULL)
        return -1;

    queue->tasks[queue->tail] = task;

    queue->tail =
        (queue->tail + 1) %
        WAIT_QUEUE_MAX_TASKS;

    queue->count++;

    task->wait_queue = queue;

    return 0;
}

struct task *wait_queue_peek(
    const struct wait_queue *queue
)
{
    if (queue == NULL ||
        !queue->initialized ||
        queue->count == 0)
    {
        return NULL;
    }

    return queue->tasks[queue->head];
}

struct task *wait_queue_dequeue(
    struct wait_queue *queue
)
{
    if (queue == NULL ||
        !queue->initialized ||
        queue->count == 0)
    {
        return NULL;
    }

    struct task *task =
        queue->tasks[queue->head];

    queue->tasks[queue->head] = NULL;

    queue->head =
        (queue->head + 1) %
        WAIT_QUEUE_MAX_TASKS;

    queue->count--;

    if (task != NULL &&
        task->wait_queue == queue)
    {
        task->wait_queue = NULL;
    }

    return task;
}

int wait_queue_remove(
    struct wait_queue *queue,
    struct task *task
)
{
    if (queue == NULL ||
        task == NULL ||
        !queue->initialized ||
        queue->count == 0)
    {
        return -1;
    }

    for (uint64_t index = 0;
         index < queue->count;
         index++)
    {
        uint64_t position =
            (queue->head + index) %
            WAIT_QUEUE_MAX_TASKS;

        if (queue->tasks[position] != task)
            continue;

        /*
         * Compact the FIFO sequence so wakeup ordering
         * of all remaining blocked tasks is preserved.
         */
        for (uint64_t shift = index;
             shift + 1 < queue->count;
             shift++)
        {
            uint64_t current =
                (queue->head + shift) %
                WAIT_QUEUE_MAX_TASKS;

            uint64_t next =
                (queue->head + shift + 1) %
                WAIT_QUEUE_MAX_TASKS;

            queue->tasks[current] =
                queue->tasks[next];
        }

        queue->tail =
            (queue->tail +
             WAIT_QUEUE_MAX_TASKS - 1) %
            WAIT_QUEUE_MAX_TASKS;

        queue->tasks[queue->tail] = NULL;
        queue->count--;

        if (task->wait_queue == queue)
            task->wait_queue = NULL;

        return 0;
    }

    return -1;
}

uint64_t wait_queue_count(
    const struct wait_queue *queue
)
{
    if (queue == NULL ||
        !queue->initialized)
    {
        return 0;
    }

    return queue->count;
}
