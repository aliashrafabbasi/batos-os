#include "dispatch.h"

#include "context.h"
#include "preempt.h"

#include "../../../sched/task.h"

/*
 * Resolve the generic continuation authority into an x86_64
 * transfer target.
 *
 * The generic scheduler records which continuation is authoritative,
 * but only this architecture layer interprets the representation.
 */
int x86_64_scheduler_resolve_target(
    struct task *task,
    struct x86_64_resume_target *target
)
{
    if (task == NULL || target == NULL)
        return -1;

    target->kind = X86_64_RESUME_NONE;
    target->context = NULL;

    if (task->resume_authority == TASK_RESUME_CONTEXT)
    {
        target->kind = X86_64_RESUME_CONTEXT;
        target->context = &task->context;
        return 0;
    }

    if (task->resume_authority == TASK_RESUME_INTERRUPT &&
        x86_64_preempt_state_is_valid(&task->preempt_state))
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame =
            (struct irq_frame *)(uintptr_t)
                task->preempt_state.frame_address;
        return 0;
    }

    return -2;
}

int x86_64_scheduler_dispatch(
    struct task *current,
    const struct x86_64_resume_target *target
)
{
    if (current == NULL || target == NULL)
        return -1;

    switch (target->kind)
    {
        case X86_64_RESUME_CONTEXT:
            if (target->context == NULL)
                return -2;

            /*
             * This primitive returns when the suspended cooperative
             * continuation is later resumed through .scheduler_resume.
             */
            x86_64_context_switch_and_enable_interrupts(
                &current->context,
                target->context
            );

            return 0;

        case X86_64_RESUME_INTERRUPT:
            if (target->frame == NULL)
                return -3;

            /*
             * iretq transfers execution to the authoritative interrupt
             * continuation. This execution path does not return here.
             */
            x86_64_context_switch_to_interrupt(
                &current->context,
                target->frame
            );

            __builtin_unreachable();

        case X86_64_RESUME_NONE:
        default:
            return -4;
    }
}
