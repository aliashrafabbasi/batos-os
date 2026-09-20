#ifndef BATOS_WAIT_QUEUE_H
#define BATOS_WAIT_QUEUE_H

#include <stdint.h>

struct task;

#define WAIT_QUEUE_MAX_TASKS 256ULL

struct wait_queue
{
    struct task *tasks[WAIT_QUEUE_MAX_TASKS];

    uint64_t head;
    uint64_t tail;
    uint64_t count;
    int initialized;
};

int wait_queue_init(
    struct wait_queue *queue
);

int wait_queue_enqueue(
    struct wait_queue *queue,
    struct task *task
);

struct task *wait_queue_peek(
    const struct wait_queue *queue
);

struct task *wait_queue_dequeue(
    struct wait_queue *queue
);

int wait_queue_remove(
    struct wait_queue *queue,
    struct task *task
);

int wait_queue_contains(
    const struct wait_queue *queue,
    const struct task *task
);

uint64_t wait_queue_count(
    const struct wait_queue *queue
);

#endif
