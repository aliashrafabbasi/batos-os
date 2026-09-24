#ifndef BATOS_EXECUTION_H
#define BATOS_EXECUTION_H

#include <stdint.h>

#include "../process/process.h"
#include "../sched/task.h"

/*
 * Create and publish one kernel-mode execution unit.
 *
 * The caller owns the Process and Task object storage.
 *
 * Execution owns only the cross-subsystem creation transaction:
 * Process + Task + registry membership + Process/Task membership
 * + READY scheduler membership.
 *
 * It does not own the VMM address space, scheduler lifetime,
 * Process lifetime after successful creation, or Task lifetime
 * after successful creation.
 */
int execution_create_kernel_task(
    struct process *process,
    struct task *task,
    uint64_t pid,
    uint64_t tid,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
);

/*
 * Create and publish an additional kernel Task inside an already
 * active Process.
 *
 * The Process and its address-space association already exist.
 * Execution owns only the Task publication transaction:
 * Task + Task Registry membership + Process/Task membership
 * + READY scheduler membership.
 */
int execution_create_task(
    struct process *process,
    struct task *task,
    uint64_t tid,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
);

#endif
