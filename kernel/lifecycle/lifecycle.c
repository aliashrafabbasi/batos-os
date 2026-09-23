#include "lifecycle.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt/irq_state.h"
#include "../sched/task.h"
#include "../sched/task_registry.h"
#include "../sched/runqueue.h"
#include "../sched/scheduler.h"
#include "../process/process.h"

static struct task *lifecycle_pending[TASK_MAX_TASKS];

static uint64_t lifecycle_head;
static uint64_t lifecycle_tail;
static uint64_t lifecycle_active_count;
static int lifecycle_initialized;

int lifecycle_init(void)
{
    for (uint64_t index = 0;
         index < TASK_MAX_TASKS;
         index++)
    {
        lifecycle_pending[index] = NULL;
    }

    lifecycle_head = 0;
    lifecycle_tail = 0;
    lifecycle_active_count = 0;
    lifecycle_initialized = 1;

    return 0;
}

int lifecycle_contains_pending(
    const struct task *task
)
{
    if (!lifecycle_initialized ||
        task == NULL)
    {
        return 0;
    }

    for (uint64_t index = 0;
         index < lifecycle_active_count;
         index++)
    {
        uint64_t position =
            (lifecycle_head + index) % TASK_MAX_TASKS;

        if (lifecycle_pending[position] == task)
            return 1;
    }

    return 0;
}

int lifecycle_publish_terminated_task(
    struct task *task
)
{
    if (!lifecycle_initialized ||
        task == NULL)
    {
        return -1;
    }

    if (task->state != TASK_STATE_TERMINATED)
        return -2;

    if (task->lifecycle_mode != TASK_LIFECYCLE_MANAGED)
        return -3;

    /*
     * Publication is only a deferred reclamation handoff.
     * A terminated current Task may therefore be published before
     * scheduler_exit_current() transfers CPU ownership.
     *
     * Actual reclamation remains forbidden while the Task is
     * scheduler-current because its execution stack is still live.
     */

    if (lifecycle_active_count >= TASK_MAX_TASKS)
        return -4;

    if (lifecycle_contains_pending(task))
        return -5;

    /*
     * A published Task must still be discoverable through the
     * Task registry. Registry release belongs to the reaper.
     */
    if (!task_registry_contains(task))
        return -6;

    lifecycle_pending[lifecycle_tail] = task;

    lifecycle_tail =
        (lifecycle_tail + 1) % TASK_MAX_TASKS;

    lifecycle_active_count++;

    return 0;
}

int lifecycle_reap_one(void)
{
    struct task *task;
    struct process *process;
    uint64_t irq_flags;

    if (!lifecycle_initialized)
        return -1;

    if (lifecycle_active_count == 0)
        return 1;

    /*
     * Lifecycle reclamation is a cross-subsystem ownership mutation.
     * Protect the complete preflight-to-retirement transaction while
     * preserving the caller's original interrupt state.
     */
    irq_flags = x86_64_irq_save();

    task = lifecycle_pending[lifecycle_head];

    if (task == NULL)
    {
        x86_64_irq_restore(irq_flags);
        return -2;
    }

    /*
     * Preflight the complete ownership contract before mutation.
     * Expected validation failure leaves every lifecycle owner
     * unchanged and preserves the pending entry for a later retry.
     */
    if (task->state != TASK_STATE_TERMINATED)
    {
        x86_64_irq_restore(irq_flags);
        return -3;
    }

    if (scheduler_get_current() == task)
    {
        x86_64_irq_restore(irq_flags);
        return -4;
    }

    if (task->resume_authority != TASK_RESUME_NONE)
    {
        x86_64_irq_restore(irq_flags);
        return -5;
    }

    if (task->wait_queue != NULL)
    {
        x86_64_irq_restore(irq_flags);
        return -6;
    }

    if (runqueue_contains(task))
    {
        x86_64_irq_restore(irq_flags);
        return -7;
    }

    if (!task_registry_contains(task))
    {
        x86_64_irq_restore(irq_flags);
        return -8;
    }

    process = task->process;

    if (process != NULL)
    {
        if (!process_contains_task(process, task))
        {
            x86_64_irq_restore(irq_flags);
            return -9;
        }
    }

    /*
     * The complete ownership contract has passed preflight.
     * Release owners in dependency order.
     */
    if (process != NULL)
    {
        if (process_detach_task(process, task) != 0)
        {
            x86_64_irq_restore(irq_flags);
            return -10;
        }
    }

    if (task_registry_unregister(task) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -11;
    }

    if (task_destroy(task) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -12;
    }

    /*
     * All Task-owned resources and external ownership references
     * have now been released successfully. Retire the lifecycle
     * publication only after reclamation has completed.
     */
    lifecycle_pending[lifecycle_head] = NULL;

    lifecycle_head =
        (lifecycle_head + 1) % TASK_MAX_TASKS;

    lifecycle_active_count--;

    x86_64_irq_restore(irq_flags);
    return 0;
}

uint64_t lifecycle_pending_count(void)
{
    if (!lifecycle_initialized)
        return 0;

    return lifecycle_active_count;
}
