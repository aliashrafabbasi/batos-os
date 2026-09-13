#include "task_registry.h"

#include "task.h"

#include <stddef.h>

static struct task *task_registry[TASK_MAX_TASKS];

static uint64_t task_registry_active_count;
static int task_registry_initialized;

int task_registry_init(void)
{
    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        task_registry[index] = NULL;
    }

    task_registry_active_count = 0;
    task_registry_initialized = 1;

    return 0;
}

int task_registry_register(
    struct task *task
)
{
    if (!task_registry_initialized ||
        task == NULL ||
        task->id == 0 ||
        task->id > TASK_MAX_TASKS)
    {
        return -1;
    }

    if (task->state == TASK_STATE_TERMINATED)
        return -1;

    uint64_t index = task->id - 1;

    if (task_registry[index] != NULL)
        return -1;

    if (task_registry_active_count >= TASK_MAX_TASKS)
        return -1;

    task_registry[index] = task;
    task_registry_active_count++;

    return 0;
}

int task_registry_unregister(
    struct task *task
)
{
    if (!task_registry_initialized ||
        task == NULL ||
        task->id == 0 ||
        task->id > TASK_MAX_TASKS)
    {
        return -1;
    }

    uint64_t index = task->id - 1;

    if (task_registry[index] != task)
        return -1;

    task_registry[index] = NULL;

    if (task_registry_active_count == 0)
        return -1;

    task_registry_active_count--;

    return 0;
}

struct task *task_registry_find(
    uint64_t id
)
{
    if (!task_registry_initialized ||
        id == 0 ||
        id > TASK_MAX_TASKS)
    {
        return NULL;
    }

    return task_registry[id - 1];
}

uint64_t task_registry_count(void)
{
    if (!task_registry_initialized)
        return 0;

    return task_registry_active_count;
}
