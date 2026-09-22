#include "scheduler.h"

#include <stddef.h>

#include "runqueue.h"
#include "../arch/x86_64/sched/context.h"
#include "../arch/x86_64/sched/dispatch.h"
#include "../arch/x86_64/interrupt/irq_state.h"

static struct task *scheduler_current = NULL;
static uint64_t scheduler_dispatch_count = 0;

struct task *scheduler_peek_next(void)
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

    if (task_transition(next, TASK_STATE_RUNNING) != 0)
    {
        runqueue_enqueue(next);
        return -5;
    }

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
    struct x86_64_resume_target target;
    uint64_t irq_flags;

    if (current == NULL)
    {
        return -1;
    }

    if (current->state != TASK_STATE_RUNNING)
    {
        return -2;
    }

    /*
     * Scheduler ownership transfer is a single-CPU critical
     * transaction. Mask maskable interrupts before inspecting
     * and committing the READY candidate so a timer interrupt
     * cannot invalidate the preflighted destination.
     */
    irq_flags = x86_64_irq_save();

    /*
     * Preflight the destination before changing current ownership.
     * The architecture layer validates continuation authority;
     * the generic scheduler only carries the resolved target.
     */
    next = scheduler_peek_next();

    if (next == NULL)
    {
        x86_64_irq_restore(irq_flags);
        return -3;
    }

    if (next->state != TASK_STATE_READY)
    {
        x86_64_irq_restore(irq_flags);
        return -4;
    }

    if (x86_64_scheduler_resolve_target(
            next,
            &target
        ) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -5;
    }

    if (task_transition(current, TASK_STATE_READY) != 0)
    {
        x86_64_irq_restore(irq_flags);
        return -6;
    }

    if (runqueue_enqueue(current) != 0)
    {
        if (task_transition(current, TASK_STATE_RUNNING) != 0)
        {
            x86_64_irq_restore(irq_flags);
            return -6;
        }

        x86_64_irq_restore(irq_flags);
        return -6;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
        {
            x86_64_irq_restore(irq_flags);
            return -7;
        }

        x86_64_irq_restore(irq_flags);
        return -7;
    }

    /*
     * The runqueue is the generic ownership authority. The
     * destination must still be the READY task whose continuation
     * was preflighted above.
     */
    if (next->state != TASK_STATE_READY)
    {
        runqueue_enqueue(next);
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
        {
            x86_64_irq_restore(irq_flags);
            return -8;
        }

        x86_64_irq_restore(irq_flags);
        return -8;
    }

    if (task_transition(next, TASK_STATE_RUNNING) != 0)
    {
        runqueue_enqueue(next);
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
        {
            x86_64_irq_restore(irq_flags);
            return -9;
        }

        x86_64_irq_restore(irq_flags);
        return -9;
    }

    scheduler_current = next;
    scheduler_dispatch_count++;

    /*
     * Ordinary cooperative execution resumes through current's
     * saved context. Any older interrupt continuation is no longer
     * authoritative for cooperative dispatch.
     */
    current->resume_authority = TASK_RESUME_CONTEXT;

    /*
     * The dispatch primitive owns the final architecture handoff.
     *
     * CONTEXT dispatch enables interrupts immediately before entering
     * the destination continuation. INTERRUPT dispatch uses the IF
     * state encoded in its authoritative interrupt frame.
     *
     * Therefore the saved irq_flags must NOT be restored on success.
     */
    if (x86_64_scheduler_dispatch(
            current,
            &target
        ) != 0)
    {
        /*
         * Returning here means the architecture handoff rejected
         * an already-preflighted target. This is an internal
         * scheduler/architecture invariant violation after generic
         * ownership has already committed.
         */
        for (;;)
            __asm__ volatile ("cli\n\thlt");
    }

    return 0;
}

int scheduler_block_current(
    struct task *task
)
{
    struct task *current = scheduler_current;
    struct task *next;
    struct x86_64_resume_target target;

    if (task == NULL ||
        task != current)
    {
        return -1;
    }

    if (current->state != TASK_STATE_BLOCKED)
    {
        return -2;
    }

    /*
     * Blocking is a terminal ownership transition for the current
     * execution path. The caller must hold the interrupt-disabled
     * critical section established by task_block().
     */
    if (x86_64_irq_is_enabled())
    {
        return -3;
    }

    /*
     * Keep scheduler ownership unchanged until the replacement
     * task has been selected and its architecture continuation
     * has been resolved successfully.
     */
    next = scheduler_peek_next();

    if (next == NULL)
    {
        return -4;
    }

    if (next->state != TASK_STATE_READY)
    {
        return -5;
    }

    if (x86_64_scheduler_resolve_target(next, &target) != 0)
    {
        return -6;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        return -7;
    }

    if (task_transition(next, TASK_STATE_RUNNING) != 0)
    {
        if (runqueue_enqueue(next) != 0)
            return -8;

        return -8;
    }

    scheduler_current = next;
    scheduler_dispatch_count++;

    if (x86_64_scheduler_dispatch(current, &target) != 0)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    return 0;
}

int scheduler_preempt_current(
    struct task *expected,
    struct task **next
)
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

    /*
     * The architecture layer validated `expected` before asking
     * the scheduler to transfer ownership. The scheduler only
     * accepts that exact FIFO head as the next owner.
     */
    if (expected == NULL || selected != expected)
    {
        return -4;
    }

    if (selected->state != TASK_STATE_READY)
    {
        return -5;
    }

    /*
     * Do not change scheduler_current until both runqueue
     * ownership transitions have succeeded.
     */
    if (task_transition(current, TASK_STATE_READY) != 0)
    {
        return -6;
    }

    if (runqueue_enqueue(current) != 0)
    {
        if (task_transition(current, TASK_STATE_RUNNING) != 0)
            return -6;

        return -6;
    }

    selected = scheduler_take_next();

    if (selected == NULL)
    {
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
            return -7;

        return -7;
    }

    /*
     * The dequeued task is the exact candidate validated by the
     * caller. No architecture-specific continuation state is
     * interpreted or modified here.
     */
    if (selected != expected)
    {
        runqueue_enqueue(selected);
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
            return -8;

        return -8;
    }

    if (task_transition(selected, TASK_STATE_RUNNING) != 0)
    {
        runqueue_enqueue(selected);
        runqueue_remove(current);

        if (task_transition(current, TASK_STATE_RUNNING) != 0)
            return -8;

        return -8;
    }

    scheduler_current = selected;
    scheduler_dispatch_count++;

    *next = selected;

    return 0;
}

int scheduler_exit_current(struct task *task)
{
    struct task *current = scheduler_current;
    struct task *next;
    struct x86_64_resume_target target;

    if (task == NULL || task != current)
    {
        return -1;
    }

    if (current->state != TASK_STATE_TERMINATED)
    {
        return -2;
    }

    /*
     * A terminated task has no valid continuation of its own.
     * The terminal handoff therefore executes with interrupts
     * disabled until architecture dispatch transfers control to
     * the selected successor.
     */
    if (x86_64_irq_is_enabled())
    {
        return -3;
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
        return -4;
    }

    if (x86_64_scheduler_resolve_target(next, &target) != 0)
    {
        return -5;
    }

    next = scheduler_take_next();

    if (next == NULL)
    {
        return -6;
    }

    if (task_transition(next, TASK_STATE_RUNNING) != 0)
    {
        if (runqueue_enqueue(next) != 0)
            return -7;

        return -7;
    }

    scheduler_current = next;
    scheduler_dispatch_count++;

    if (x86_64_scheduler_dispatch(current, &target) != 0)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    return 0;
}

uint64_t scheduler_get_dispatch_count(void)
{
    return scheduler_dispatch_count;
}
