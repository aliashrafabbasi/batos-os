#include "process.h"

#include "process_registry.h"
#include "../mm/vmm/vmm.h"
#include <stddef.h>

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
     * address space. Process creation only accepts a newly
     * created address space; activation is an execution concern.
     */
    if (vmm_get_address_space_state(
            address_space,
            &address_space_state
        ) != 0)
    {
        return -4;
    }

    /*
     * A newly created Process may only associate with a
     * VMM-created address space in the CREATED state.
     * Address-space activation is an execution concern and
     * must not be performed implicitly by Process creation.
     */
    if (address_space_state !=
        VMM_ADDRESS_SPACE_CREATED)
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
     * Process v1 deliberately does not destroy the VMM
     * address space. VMM owns address-space lifetime, while
     * Thread membership/reaping and address-space release
     * semantics are established by later clusters.
     */
    if (process_registry_unregister(process) != 0)
        return -4;

    process->id = 0;
    process->state = PROCESS_STATE_NEW;
    process->address_space = 0;

    return 0;
}
