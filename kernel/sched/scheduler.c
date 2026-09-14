#include "scheduler.h"

#include <stddef.h>

#include "runqueue.h"
#include "../arch/x86_64/sched/context.h"

static struct task *scheduler_current = NULL;
static uint64_t scheduler_dispatch_count = 0;

static struct task *scheduler_peek_next(void)
{
    return runqueue_peek();
}

static struct task *scheduler_take_next(void)
{
    return runqueue_dequeue();
}

int scheduler_init(void)
{
    scheduler_current = NULL;
    scheduler_dispatch_count = 0;

    if (runqueue_init() != 0)
        return -1;

    return task_set_exit_handler(
        scheduler_exit_current
    );
}

struct task *scheduler_get_current(void)
{
    return scheduler_current;
}

int scheduler_start(void)
{
    struct task *next;

    if (scheduler_current != NULL)
    {
        return -1;
    }

    next = scheduler_peek_next();

    if (next == NULL)
    {
        return -2;
    }

    if (next->state != TASK_STATE_READY)
    {
        return -3;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        return -4;
    }

    next->state = TASK_STATE_RUNNING;
    scheduler_current = next;

    return 0;
}

int scheduler_add(struct task *task)
{
    if (task == NULL)
    {
        return -1;
    }

    if (task->state != TASK_STATE_READY)
    {
        return -2;
    }

    return runqueue_enqueue(task);
}

int scheduler_yield(void)
{
    struct task *current = scheduler_current;
    struct task *next;

    if (current == NULL)
    {
        return -1;
    }

    if (current->state != TASK_STATE_RUNNING)
    {
        return -2;
    }

    current->state = TASK_STATE_READY;

    if (runqueue_enqueue(current) != 0)
    {
        current->state = TASK_STATE_RUNNING;
        return -3;
    }

    next = scheduler_peek_next();

    if (next == NULL)
    {
        runqueue_remove(current);
        current->state = TASK_STATE_RUNNING;
        return -4;
    }

    if (next->state != TASK_STATE_READY)
    {
        runqueue_remove(current);
        current->state = TASK_STATE_RUNNING;
        return -5;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        runqueue_remove(current);
        current->state = TASK_STATE_RUNNING;
        return -6;
    }

    next->state = TASK_STATE_RUNNING;
    scheduler_current = next;
    scheduler_dispatch_count++;

    x86_64_context_switch(
        &current->context,
        &next->context
    );

    return 0;
}

int scheduler_preempt_current(struct task **next)
{
    struct task *current = scheduler_current;
    struct task *selected;

    if (next == NULL)
    {
        return -1;
    }

    *next = NULL;

    if (current == NULL)
    {
        return -2;
    }

    if (current->state != TASK_STATE_RUNNING)
    {
        return -3;
    }

    /*
     * A preemption request does not require a switch when no
     * alternative READY task exists. Keep the current task as
     * RUNNING and leave runqueue ownership unchanged.
     */
    selected = scheduler_peek_next();

    if (selected == NULL)
    {
        *next = current;
        return 1;
    }

    if (selected->state != TASK_STATE_READY)
    {
        return -4;
    }

    /*
     * Do not change scheduler_current until both runqueue
     * ownership transitions have succeeded.
     */
    current->state = TASK_STATE_READY;

    if (runqueue_enqueue(current) != 0)
    {
        current->state = TASK_STATE_RUNNING;
        return -5;
    }

    selected = scheduler_take_next();

    if (selected == NULL)
    {
        runqueue_remove(current);
        current->state = TASK_STATE_RUNNING;
        return -6;
    }

    /*
     * selected was validated as READY before dequeue. The runqueue
     * owns the FIFO transition; no architecture state is touched.
     */
    selected->state = TASK_STATE_RUNNING;
    scheduler_current = selected;
    scheduler_dispatch_count++;

    *next = selected;

    return 0;
}

int scheduler_exit_current(struct task *task)
{
    struct task *current = scheduler_current;
    struct task *next;

    if (task == NULL || task != current)
    {
        return -1;
    }

    if (current->state != TASK_STATE_TERMINATED)
    {
        return -2;
    }

    next = scheduler_peek_next();

    if (next == NULL)
    {
        /*
         * No runnable task remains. The terminated task cannot
         * return through its own stack, so there is no valid
         * continuation. Halt until a future scheduler/reaper
         * integration provides one.
         */
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    if (next->state != TASK_STATE_READY)
    {
        return -3;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        return -4;
    }

    next->state = TASK_STATE_RUNNING;
    scheduler_current = next;
    scheduler_dispatch_count++;

    x86_64_context_switch(
        &current->context,
        &next->context
    );

    return 0;
}

uint64_t scheduler_get_dispatch_count(void)
{
    return scheduler_dispatch_count;
}
