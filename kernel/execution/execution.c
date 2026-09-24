#include "execution.h"

#include "../mm/vmm/vmm.h"
#include "../process/process_registry.h"
#include "../sched/task_registry.h"
#include "../sched/runqueue.h"
#include "../sched/scheduler.h"

static int execution_rollback_task_internal(
    struct process *process,
    struct task *task,
    int task_registered,
    int task_attached
)
{
    if (task_attached)
    {
        if (process_detach_task(process, task) != 0)
            return -1;
    }

    if (task_registered)
    {
        if (task_registry_unregister(task) != 0)
            return -1;
    }

    if (task_destroy(task) != 0)
        return -1;

    return 0;
}

static int execution_rollback_kernel_task_internal(
    struct process *process,
    struct task *task,
    int task_registered,
    int task_attached
)
{
    if (execution_rollback_task_internal(
            process,
            task,
            task_registered,
            task_attached
        ) != 0)
    {
        return -1;
    }

    if (process_terminate(process) != 0)
        return -1;

    if (process_destroy(process) != 0)
        return -1;

    return 0;
}

static int execution_preflight_destroy_task(
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
     * This cleanup boundary is valid only for a successfully
     * published READY execution unit that has never entered
     * execution.
     */
    if (task->state != TASK_STATE_READY ||
        task->process != process ||
        !task_registry_contains(task) ||
        !process_registry_contains(process) ||
        !runqueue_contains(task))
    {
        return -2;
    }

    /*
     * Validate Task-local execution/resource state while external
     * ownership is still intentionally present.
     */
    if (task_validate_reclaim(task) != 0)
        return -3;

    return 0;
}

static int execution_preflight_destroy_kernel_task(
    struct process *process,
    struct task *task
)
{
    if (execution_preflight_destroy_task(
            process,
            task
        ) != 0)
    {
        return -1;
    }

    /*
     * The kernel-task creation transaction owns exactly one Task.
     * Process destruction therefore requires exactly one current
     * Process membership.
     */
    if (process->state != PROCESS_STATE_ACTIVE ||
        process->task_count != 1)
    {
        return -2;
    }

    return 0;
}

int execution_destroy_task(
    struct process *process,
    struct task *task
)
{
    if (execution_preflight_destroy_task(
            process,
            task
        ) != 0)
    {
        return -1;
    }

    /*
     * Complete preflight has succeeded. The following operations
     * are the ownership-release commit and therefore are not
     * treated as ordinary recoverable transaction failures.
     *
     * Publication order:
     *     Task Registry -> Process -> Scheduler
     *
     * Teardown order:
     *     Scheduler -> Process -> Task Registry -> Task.
     */
    if (runqueue_remove(task) != 0)
        return -2;

    if (process_detach_task(
            process,
            task
        ) != 0)
    {
        return -3;
    }

    if (task_registry_unregister(task) != 0)
        return -4;

    if (task_destroy(task) != 0)
        return -5;

    return 0;
}

int execution_destroy_kernel_task(
    struct process *process,
    struct task *task
)
{
    if (execution_preflight_destroy_kernel_task(
            process,
            task
        ) != 0)
    {
        return -1;
    }

    if (execution_destroy_task(
            process,
            task
        ) != 0)
    {
        /*
         * The complete preflight has already succeeded.
         * Failure here therefore indicates an internal invariant
         * violation rather than an expected rollback condition.
         */
        return -2;
    }

    if (process_terminate(process) != 0)
        return -3;

    if (process_destroy(process) != 0)
        return -4;

    return 0;
}

int execution_create_kernel_task(
    struct process *process,
    struct task *task,
    uint64_t pid,
    uint64_t tid,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
)
{
    int task_registered = 0;
    int task_attached = 0;

    if (process == NULL ||
        task == NULL ||
        pid == 0 ||
        tid == 0 ||
        address_space == 0 ||
        entry == NULL)
    {
        return -1;
    }

    /*
     * Kernel-mode Task stacks currently exist only in the BATOS
     * kernel address space. Address-space switching belongs to a
     * later execution/VM integration milestone.
     *
     * Execution therefore requires the established kernel PML4
     * for this v1 kernel-task constructor.
     */
    if (address_space != vmm_get_pml4())
        return -2;

    /*
     * Process creation establishes Process registry ownership
     * before any Task ownership is published.
     */
    if (process_create(
            process,
            pid,
            address_space
        ) != 0)
    {
        return -4;
    }

    /*
     * task_create() establishes the complete kernel execution
     * object and its first-run continuation, but deliberately
     * does not publish registry, Process, or scheduler ownership.
     */
    if (task_create(
            task,
            tid,
            address_space,
            entry,
            argument
        ) != 0)
    {
        if (process_terminate(process) != 0 ||
            process_destroy(process) != 0)
        {
            return -5;
        }

        return -6;
    }

    /*
     * Task identity becomes externally discoverable before
     * Process membership is published.
     */
    if (task_registry_register(task) != 0)
    {
        if (execution_rollback_kernel_task_internal(
                process,
                task,
                0,
                0
            ) != 0)
        {
            return -5;
        }

        return -7;
    }

    task_registered = 1;

    /*
     * Process membership is reciprocal and validates the
     * Process/Task address-space invariant.
     */
    if (process_attach_task(
            process,
            task
        ) != 0)
    {
        if (execution_rollback_kernel_task_internal(
                process,
                task,
                task_registered,
                0
            ) != 0)
        {
            return -5;
        }

        return -8;
    }

    task_attached = 1;

    /*
     * READY runqueue membership is the final publication point.
     * A successful return means the execution unit is fully
     * owned by all required subsystems and is schedulable.
     */
    if (scheduler_add(task) != 0)
    {
        if (execution_rollback_kernel_task_internal(
                process,
                task,
                task_registered,
                task_attached
            ) != 0)
        {
            return -5;
        }

        return -9;
    }

    /*
     * All required execution ownership domains are now published:
     * Process, Task Registry, and Scheduler. Only at this point does
     * terminal execution acquire the production lifecycle contract.
     *
     * Keeping this assignment after scheduler admission guarantees
     * that every rollback path before this point remains unmanaged.
     */
    task->lifecycle_mode =
        TASK_LIFECYCLE_MANAGED;

    return 0;
}

int execution_create_task(
    struct process *process,
    struct task *task,
    uint64_t tid,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
)
{
    int task_registered = 0;
    int task_attached = 0;

    if (process == NULL ||
        task == NULL ||
        tid == 0 ||
        address_space == 0 ||
        entry == NULL)
    {
        return -1;
    }

    /*
     * Additional Tasks must join an already published Process.
     * Execution does not create or activate a new address space.
     */
    if (!process_registry_contains(process) ||
        process->state != PROCESS_STATE_ACTIVE)
    {
        return -2;
    }

    if (address_space != process->address_space)
        return -3;

    if (address_space != vmm_get_pml4())
        return -4;

    /*
     * task_create() constructs only the execution object.
     */
    if (task_create(
            task,
            tid,
            address_space,
            entry,
            argument
        ) != 0)
    {
        return -5;
    }

    /*
     * Task identity becomes externally discoverable before
     * Process membership is published.
     */
    if (task_registry_register(task) != 0)
    {
        if (execution_rollback_task_internal(
                process,
                task,
                0,
                0
            ) != 0)
        {
            return -6;
        }

        return -7;
    }

    task_registered = 1;

    /*
     * Process membership establishes the reciprocal ownership
     * invariant and validates the address-space relationship.
     */
    if (process_attach_task(
            process,
            task
        ) != 0)
    {
        if (execution_rollback_task_internal(
                process,
                task,
                task_registered,
                0
            ) != 0)
        {
            return -6;
        }

        return -8;
    }

    task_attached = 1;

    /*
     * READY scheduler ownership is the final publication point.
     */
    if (scheduler_add(task) != 0)
    {
        if (execution_rollback_task_internal(
                process,
                task,
                task_registered,
                task_attached
            ) != 0)
        {
            return -6;
        }

        return -9;
    }

    /*
     * Terminal execution now participates in deferred
     * production lifecycle reclamation.
     */
    task->lifecycle_mode =
        TASK_LIFECYCLE_MANAGED;

    return 0;
}
