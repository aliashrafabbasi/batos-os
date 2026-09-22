#ifndef BATOS_PROCESS_REGISTRY_H
#define BATOS_PROCESS_REGISTRY_H

#include <stdint.h>

struct process;

/*
 * Process registry owns PID identity/lifecycle membership only.
 *
 * It does not own process memory, address spaces, page tables,
 * threads, kernel stacks, or scheduling.
 */
int process_registry_init(void);

int process_registry_register(
    struct process *process
);

int process_registry_unregister(
    struct process *process
);

int process_registry_contains(
    const struct process *process
);

struct process *process_registry_find(
    uint64_t pid
);

struct process *process_registry_find_by_address_space(
    uint64_t address_space
);

uint64_t process_registry_count(void);

#endif
