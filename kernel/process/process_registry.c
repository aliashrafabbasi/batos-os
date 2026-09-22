#include "process_registry.h"

#include "process.h"
#include <stddef.h>

static struct process *
    process_registry[PROCESS_MAX_PROCESSES];

static uint64_t process_registry_count_value = 0;
static uint8_t process_registry_initialized = 0;

int process_registry_init(void)
{
    for (uint64_t i = 0;
         i < PROCESS_MAX_PROCESSES;
         i++)
    {
        process_registry[i] = NULL;
    }

    process_registry_count_value = 0;
    process_registry_initialized = 1;

    return 0;
}

int process_registry_register(
    struct process *process
)
{
    if (!process_registry_initialized ||
        process == NULL)
    {
        return -1;
    }

    /*
     * Registry membership is pointer ownership, not merely
     * PID-slot occupancy. Reject re-registration of an object
     * that is already owned by the registry before inspecting
     * mutable Process identity fields.
     */
    for (uint64_t i = 0;
         i < PROCESS_MAX_PROCESSES;
         i++)
    {
        if (process_registry[i] == process)
            return -2;
    }

    if (process->id == 0 ||
        process->id > PROCESS_MAX_PROCESSES)
    {
        return -3;
    }

    if (process->state == PROCESS_STATE_TERMINATED)
    {
        return -4;
    }

    uint64_t index = process->id - 1;

    if (process_registry[index] != NULL)
    {
        return -5;
    }

    process_registry[index] = process;
    process_registry_count_value++;

    return 0;
}

int process_registry_unregister(
    struct process *process
)
{
    if (!process_registry_initialized ||
        process == NULL)
    {
        return -1;
    }

    if (process->id == 0 ||
        process->id > PROCESS_MAX_PROCESSES)
    {
        return -2;
    }

    uint64_t index = process->id - 1;

    if (process_registry[index] != process)
    {
        return -3;
    }

    /*
     * Validate the registry count before mutating membership.
     * A successful unregister must never partially commit and
     * then report failure.
     */
    if (process_registry_count_value == 0)
        return -4;

    process_registry[index] = NULL;
    process_registry_count_value--;

    return 0;
}

int process_registry_contains(
    const struct process *process
)
{
    if (!process_registry_initialized ||
        process == NULL)
    {
        return 0;
    }

    if (process->id == 0 ||
        process->id > PROCESS_MAX_PROCESSES)
    {
        return 0;
    }

    return process_registry[process->id - 1] == process;
}

struct process *process_registry_find(
    uint64_t pid
)
{
    if (!process_registry_initialized ||
        pid == 0 ||
        pid > PROCESS_MAX_PROCESSES)
    {
        return NULL;
    }

    return process_registry[pid - 1];
}

struct process *process_registry_find_by_address_space(
    uint64_t address_space
)
{
    if (!process_registry_initialized ||
        address_space == 0)
    {
        return NULL;
    }

    for (uint64_t i = 0;
         i < PROCESS_MAX_PROCESSES;
         i++)
    {
        struct process *process =
            process_registry[i];

        if (process != NULL &&
            process->address_space == address_space)
        {
            return process;
        }
    }

    return NULL;
}

uint64_t process_registry_count(void)
{
    return process_registry_count_value;
}
