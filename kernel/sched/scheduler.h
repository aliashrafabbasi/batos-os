#ifndef BATOS_SCHEDULER_H
#define BATOS_SCHEDULER_H

#include <stdint.h>

#include "task.h"

int scheduler_init(void);

struct task *scheduler_get_current(void);

int scheduler_start(void);

int scheduler_add(struct task *task);

int scheduler_yield(void);

int scheduler_exit_current(struct task *task);

uint64_t scheduler_get_dispatch_count(void);

#endif
