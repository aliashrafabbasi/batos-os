#ifndef BATOS_TASK_H
#define BATOS_TASK_H

#include <stdint.h>

#include "../arch/x86_64/sched/context.h"

#define TASK_KERNEL_STACK_PAGE_COUNT 4ULL
#define TASK_KERNEL_STACK_SIZE \
    (TASK_KERNEL_STACK_PAGE_COUNT * 4096ULL)

#define TASK_KERNEL_STACK_GUARD_SIZE 4096ULL

#define TASK_MAX_TASKS 256ULL

#define TASK_KERNEL_STACK_SLOT_SIZE \
    (TASK_KERNEL_STACK_SIZE + TASK_KERNEL_STACK_GUARD_SIZE)

enum task_state
{
    TASK_STATE_NEW = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SLEEPING,
    TASK_STATE_TERMINATED
};

typedef void (*task_entry_t)(void *argument);

struct task
{
    uint64_t id;
    enum task_state state;

    uint64_t kernel_stack_base;
    uint64_t kernel_stack_top;

    /*
     * One physical frame per stack page.
     *
     * The pages are independently owned by PMM and do not
     * have to be physically contiguous.
     */
    uint64_t kernel_stack_pages[TASK_KERNEL_STACK_PAGE_COUNT];

    /*
     * Reference to the address space containing this task.
     * Address-space lifetime remains owned by VMM.
     */
    uint64_t address_space;

    task_entry_t entry;
    void *argument;

    struct x86_64_context context;
};

int task_create(
    struct task *task,
    uint64_t id,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
);

int task_destroy(
    struct task *task
);

#endif
