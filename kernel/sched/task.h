#ifndef BATOS_TASK_H
#define BATOS_TASK_H

#include <stdint.h>

enum task_state
{
    TASK_STATE_NEW = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SLEEPING,
    TASK_STATE_TERMINATED
};

struct task
{
    uint64_t id;
    enum task_state state;

    uint64_t kernel_stack_base;
    uint64_t kernel_stack_top;

    uint64_t address_space;

    void *context;
};

#endif
