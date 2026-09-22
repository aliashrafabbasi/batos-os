#ifndef BATOS_PROCESS_H
#define BATOS_PROCESS_H

#include <stdint.h>

#define PROCESS_MAX_PROCESSES 256ULL

enum process_state
{
    PROCESS_STATE_NEW = 0,
    PROCESS_STATE_ACTIVE,
    PROCESS_STATE_TERMINATED
};

/*
 * Process owns process identity and lifecycle state.
 *
 * The address-space field is a reference to a VMM-owned
 * address space. Process does not own or directly destroy
 * page-table memory.
 *
 * Thread membership is intentionally not part of this
 * first process-foundation cluster. It will be introduced
 * by the Process <-> Thread ownership cluster.
 */
struct process
{
    uint64_t id;
    enum process_state state;
    uint64_t address_space;
};

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
