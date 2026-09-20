#ifndef BATOS_SCHEDULER_H
#define BATOS_SCHEDULER_H

#include <stdint.h>

#include "task.h"

int scheduler_init(void);

struct task *scheduler_get_current(void);

/*
 * Inspect the next READY scheduler candidate without changing
 * scheduler ownership, task state, or runqueue ownership.
 *
 * Returns:
 *   non-NULL - next READY candidate
 *   NULL     - no READY candidate exists
 */
struct task *scheduler_peek_next(void);

int scheduler_start(void);

int scheduler_add(struct task *task);

int scheduler_yield(void);

/*
 * Relinquish CPU ownership from a current task that has already
 * transitioned to BLOCKED and has established its blocking
 * ownership elsewhere.
 *
 * The scheduler selects the next READY task and performs the
 * cooperative context handoff. The scheduler does not own or
 * interpret the blocking object.
 */
int scheduler_block_current(
    struct task *task
);

/*
 * Perform the scheduler ownership transition for a preempted
 * current task without performing an architecture context switch.
 *
 * `expected` must be the READY task previously observed by the
 * caller as the next scheduler candidate.
 *
 * Return values:
 *   0  - expected task was selected
 *   1  - no alternative READY task exists; current continues
 *  <0  - scheduler error
 *
 * On success, *next is the task that owns the next CPU execution
 * context. The architecture layer owns the actual continuation
 * handoff.
 */
int scheduler_preempt_current(
    struct task *expected,
    struct task **next
);

int scheduler_exit_current(struct task *task);

uint64_t scheduler_get_dispatch_count(void);

#endif
