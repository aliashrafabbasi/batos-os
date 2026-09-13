#ifndef BATOS_RUNQUEUE_H
#define BATOS_RUNQUEUE_H

#include <stdint.h>

struct task;

int runqueue_init(void);

int runqueue_enqueue(
    struct task *task
);

struct task *runqueue_dequeue(void);

int runqueue_remove(
    struct task *task
);

int runqueue_contains(
    const struct task *task
);

uint64_t runqueue_count(void);

#endif
