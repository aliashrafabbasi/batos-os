#ifndef BATOS_PROCESS_H
#define BATOS_PROCESS_H

#include <stdint.h>

#define PROCESS_MAX_PROCESSES 256ULL
#define PROCESS_MAX_TASKS 256ULL

struct task;

enum process_state
{
    PROCESS_STATE_NEW = 0,
    PROCESS_STATE_ACTIVE,
    PROCESS_STATE_TERMINATED
};

/*
 * Process owns process identity, lifecycle state, and
 * membership of its Tasks.
 *
 * The address-space field is a reference to a VMM-owned
 * address space. Process does not own or directly destroy
 * page-table memory.
 *
 * A Process owns zero or more Task membership references.
 * Task execution/scheduling state remains owned by Task and
 * the scheduler subsystem respectively.
 */
struct process
{
    uint64_t id;
    enum process_state state;
    uint64_t address_space;

    struct task *tasks[PROCESS_MAX_TASKS];
    uint64_t task_count;
};

int process_attach_task(
    struct process *process,
    struct task *task
);

int process_detach_task(
    struct process *process,
    struct task *task
);

int process_contains_task(
    const struct process *process,
    const struct task *task
);

uint64_t process_task_count(
    const struct process *process
);

int process_create(
    struct process *process,
    uint64_t pid,
    uint64_t address_space
);

int process_transition(
    struct process *process,
    enum process_state target
);

int process_terminate(
    struct process *process
);

int process_destroy(
    struct process *process
);

#endif
