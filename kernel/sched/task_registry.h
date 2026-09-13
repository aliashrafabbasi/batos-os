#ifndef BATOS_TASK_REGISTRY_H
#define BATOS_TASK_REGISTRY_H

#include <stdint.h>

struct task;

/*
 * Global task registry.
 *
 * The registry owns task identity and lifecycle membership only.
 * It does not own task memory, stacks, address spaces, or scheduling.
 */
int task_registry_init(void);

int task_registry_register(
    struct task *task
);

int task_registry_unregister(
    struct task *task
);

struct task *task_registry_find(
    uint64_t id
);

uint64_t task_registry_count(void);

#endif
