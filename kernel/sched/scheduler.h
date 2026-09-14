#ifndef BATOS_SCHEDULER_H
#define BATOS_SCHEDULER_H

#include <stdint.h>

#include "task.h"

int scheduler_init(void);

struct task *scheduler_get_current(void);

int scheduler_start(void);

int scheduler_add(struct task *task);

int scheduler_yield(void);

/*
 * Perform the scheduler ownership transition for a preempted
 * current task without performing an architecture context switch.
 *
 * Return values:
 *   0  - another READY task was selected
 *   1  - no alternative READY task exists; current continues
 *  <0  - scheduler error
 *
 * On success, *next is the task that owns the next CPU execution
 * context. The architecture layer owns the actual interrupt-frame
 * handoff.
 */
int scheduler_preempt_current(struct task **next);

int scheduler_exit_current(struct task *task);

uint64_t scheduler_get_dispatch_count(void);

#endif
