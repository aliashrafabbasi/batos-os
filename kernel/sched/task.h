#ifndef BATOS_TASK_H
#define BATOS_TASK_H

#include <stdint.h>

#include "../arch/x86_64/sched/context.h"
#include "../arch/x86_64/sched/preempt.h"

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

/*
 * Identifies the authoritative execution continuation owned by
 * a READY task.
 *
 * The generic scheduler owns this authority value but does not
 * interpret architecture-specific saved state.
 */
enum task_resume_authority
{
    TASK_RESUME_NONE = 0,
    TASK_RESUME_CONTEXT,
    TASK_RESUME_INTERRUPT
};

struct task;

typedef void (*task_entry_t)(void *argument);
typedef int (*task_exit_handler_t)(struct task *task);

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

    /*
     * Identifies which architecture-owned continuation is
     * authoritative when this task is resumed.
     */
    enum task_resume_authority resume_authority;

    /*
     * Architecture-owned resumable interrupt-return state.
     *
     * Generic task/scheduler code owns the handle but does
     * not interpret the saved interrupt frame.
     */
    struct x86_64_preempt_state preempt_state;

    struct x86_64_context context;
};

int task_create(
    struct task *task,
    uint64_t id,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
);

int task_transition(
    struct task *task,
    enum task_state new_state
);

int task_set_exit_handler(
    task_exit_handler_t handler
);

int task_exit(
    struct task *task
);

int task_destroy(
    struct task *task
);

#endif
