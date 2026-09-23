#include "process.h"

#include "process_registry.h"
#include "../sched/task.h"
#include "../mm/vmm/vmm.h"
#include <stddef.h>


static int process_find_task_index(
    const struct process *process,
    const struct task *task,
    uint64_t *index
)
{
    if (process == NULL ||
        task == NULL ||
        index == NULL)
    {
        return -1;
    }

    for (uint64_t i = 0;
         i < process->task_count;
         i++)
    {
        if (process->tasks[i] == task)
        {
            *index = i;
            return 0;
        }
    }

    return -1;
}

int process_attach_task(
    struct process *process,
    struct task *task
)
{
    if (process == NULL ||
        task == NULL)
    {
        return -1;
    }

    /*
     * Task membership is owned by an active Process identity.
     * Do not allow an unregistered Process object to acquire
     * Task ownership.
     */
    if (!process_registry_contains(process) ||
        process->state != PROCESS_STATE_ACTIVE)
    {
        return -2;
    }

    /*
     * A Task belongs to at most one Process.
     */
    if (task->process != NULL)
        return -3;

    /*
     * The Process <-> Task relationship must not silently
     * rewrite either side's address-space reference.
     */
    if (task->address_space != process->address_space)
        return -4;

    /*
     * Capacity is checked before any ownership mutation.
     */
    if (process->task_count >= PROCESS_MAX_TASKS)
        return -5;

    /*
     * Defensive duplicate check. A correctly maintained
     * back-reference already prevents duplicates, but both
     * directions are validated at the ownership boundary.
     */
    if (process_find_task_index(
            process,
            task,
            &(uint64_t){0}
        ) == 0)
    {
        return -6;
    }

    uint64_t index = process->task_count;

    process->tasks[index] = task;
    process->task_count++;

    /*
     * Establish the reciprocal ownership reference only after
     * Process membership has been successfully reserved.
     */
    task->process = process;

    return 0;
}

int process_detach_task(
    struct process *process,
    struct task *task
)
{
    if (process == NULL ||
        task == NULL)
    {
        return -1;
    }

    if (!process_registry_contains(process))
        return -2;

    uint64_t index = 0;

    if (process_find_task_index(
            process,
            task,
            &index
        ) != 0)
    {
        return -3;
    }

    /*
     * Both sides of the relationship must agree before
     * ownership is released.
     */
    if (task->process != process)
        return -4;

    /*
     * Validate membership count before mutation.
     */
    if (process->task_count == 0)
        return -5;

    uint64_t last =
        process->task_count - 1;

    /*
     * Compact the fixed membership array. Ordering of Process
     * Task membership is not a scheduler/runqueue contract.
     */
    if (index != last)
        process->tasks[index] =
            process->tasks[last];

    process->tasks[last] = NULL;
    process->task_count--;

    /*
     * Release the reciprocal ownership reference only after
     * Process membership has been removed.
     */
    task->process = NULL;

    return 0;
}

int process_contains_task(
    const struct process *process,
    const struct task *task
)
{
    if (process == NULL ||
        task == NULL)
    {
        return 0;
    }

    uint64_t index = 0;

    return process_find_task_index(
        process,
        task,
        &index
    ) == 0 &&
    task->process == process;
}

uint64_t process_task_count(
    const struct process *process
)
{
    if (process == NULL)
        return 0;

    return process->task_count;
}

static int process_state_transition_valid(
    enum process_state current,
    enum process_state target
)
{
    if (current == PROCESS_STATE_NEW &&
        target == PROCESS_STATE_ACTIVE)
    {
        return 1;
    }

    if (current == PROCESS_STATE_ACTIVE &&
        target == PROCESS_STATE_TERMINATED)
    {
        return 1;
    }

    return 0;
}

int process_transition(
    struct process *process,
    enum process_state target
)
{
    if (process == NULL)
        return -1;

    if (!process_state_transition_valid(
            process->state,
            target
        ))
    {
        return -2;
    }

    /*
     * ACTIVE and TERMINATED are lifecycle states of a
     * registered Process identity. Prevent state mutation
     * from bypassing Process Registry ownership.
     */
    if ((target == PROCESS_STATE_ACTIVE ||
         target == PROCESS_STATE_TERMINATED) &&
        !process_registry_contains(process))
    {
        return -3;
    }

    process->state = target;

    return 0;
}

int process_create(
    struct process *process,
    uint64_t pid,
    uint64_t address_space
)
{
    uint8_t address_space_state = 0;

    if (process == NULL)
        return -1;

    /*
     * A registry-owned Process object has established identity
     * and lifecycle ownership. It must never be repurposed by
     * process_create(); destruction/reclamation must happen
     * through the Process lifecycle first.
     */
    if (process_registry_contains(process))
        return -9;

    if (pid == 0 ||
        pid > PROCESS_MAX_PROCESSES)
    {
        return -2;
    }

    if (address_space == 0)
        return -3;

    /*
     * The address space must be a currently registered VMM
     * address space. Process creation may associate with either
     * a newly created address space or an already-active address
     * space. Process creation never performs CR3 activation.
     */
    if (vmm_get_address_space_state(
            address_space,
            &address_space_state
        ) != 0)
    {
        return -4;
    }

    /*
     * CREATED and ACTIVE are both valid association states.
     *
     * CREATED:
     *   The Process references an address space that has not yet
     *   been activated by VMM.
     *
     * ACTIVE:
     *   The Process references an address space that is already
     *   active. This is required for kernel execution units whose
     *   established address space is already the active CR3.
     *
     * Process creation remains a logical ownership/association
     * operation and never activates or switches an address space.
     */
    if (address_space_state != VMM_ADDRESS_SPACE_CREATED &&
        address_space_state != VMM_ADDRESS_SPACE_ACTIVE)
    {
        return -5;
    }

    /*
     * v1 establishes a one-process/one-address-space logical
     * association. This is not VMM ownership or reference
     * counting; it simply prevents ambiguous Process identity
     * over the same address space.
     */
    if (process_registry_find_by_address_space(
            address_space
        ) != NULL)
    {
        return -6;
    }

    process->id = pid;
    process->state = PROCESS_STATE_NEW;
    process->address_space = address_space;

    /*
     * Process creation establishes a fresh empty membership
     * domain. Legitimate Task membership can only exist while
     * the Process is registry-owned.
     */
    process->task_count = 0;

    for (uint64_t i = 0;
         i < PROCESS_MAX_TASKS;
         i++)
    {
        process->tasks[i] = NULL;
    }

    if (process_registry_register(process) != 0)
    {
        process->id = 0;
        process->state = PROCESS_STATE_NEW;
        process->address_space = 0;

        return -7;
    }

    if (process_transition(
            process,
            PROCESS_STATE_ACTIVE
        ) != 0)
    {
        process_registry_unregister(process);

        process->id = 0;
        process->state = PROCESS_STATE_NEW;
        process->address_space = 0;

        return -8;
    }

    return 0;
}

int process_terminate(
    struct process *process
)
{
    if (process == NULL)
        return -1;

    if (!process_registry_contains(process))
        return -2;

    return process_transition(
        process,
        PROCESS_STATE_TERMINATED
    );
}

int process_destroy(
    struct process *process
)
{
    if (process == NULL)
        return -1;

    if (!process_registry_contains(process))
        return -2;

    if (process->state !=
        PROCESS_STATE_TERMINATED)
    {
        return -3;
    }

    /*
     * A Process cannot be destroyed while it still owns Task
     * membership. Reaping/detachment is a separate lifecycle
     * responsibility and must complete before Process identity
     * is reclaimed.
     */
    if (process->task_count != 0)
        return -4;

    /*
     * Process v1 deliberately does not destroy the VMM
     * address space. VMM owns address-space lifetime, while
     * Thread membership/reaping and address-space release
     * semantics are established by later clusters.
     */
    if (process_registry_unregister(process) != 0)
        return -5;

    process->id = 0;
    process->state = PROCESS_STATE_NEW;
    process->address_space = 0;

    process->task_count = 0;

    for (uint64_t i = 0;
         i < PROCESS_MAX_TASKS;
         i++)
    {
        process->tasks[i] = NULL;
    }

    return 0;
}
