#include "preempt.h"

#include "context.h"
#include "../cpu/gdt.h"
#include "../interrupt/irq.h"

#include "../../../sched/task.h"
#include "../../../sched/scheduler.h"

#include <stdint.h>
#include <stddef.h>

extern void x86_64_task_bootstrap_trampoline(void);

#define X86_64_PREEMPT_INITIAL_RFLAGS 0x202ULL

static uint64_t preempt_enabled = 0;

static struct irq_frame *preempt_frame_from_state(
    const struct x86_64_preempt_state *state
)
{
    if (state == NULL || state->valid == 0)
        return NULL;

    if (state->frame_address == 0)
        return NULL;

    return (struct irq_frame *)(uintptr_t)state->frame_address;
}

int x86_64_preempt_state_is_valid(
    const struct x86_64_preempt_state *state
)
{
    return preempt_frame_from_state(state) != NULL;
}

int x86_64_preempt_prepare_first_run(
    struct task *task,
    struct x86_64_preempt_state *state
)
{
    if (task == NULL || state == NULL)
        return -1;

    if (task->kernel_stack_top == 0 ||
        task->kernel_stack_base == 0)
    {
        return -1;
    }

    /*
     * Keep the existing cooperative bootstrap ABI intact:
     *
     *   [T-24] = bootstrap return slot
     *   [T-16] = struct task *
     *
     * The synthetic interrupt-return frame lives immediately
     * below that context.
     */
    uint64_t bootstrap_rsp =
        task->kernel_stack_top -
        3ULL * sizeof(uint64_t);

    uint64_t frame_address =
        bootstrap_rsp -
        sizeof(struct irq_frame);

    if (frame_address < task->kernel_stack_base)
        return -1;

    if ((frame_address & 0xFULL) != 0)
        return -1;

    struct irq_frame *frame =
        (struct irq_frame *)(uintptr_t)frame_address;

    frame->r15 = 0;
    frame->r14 = 0;
    frame->r13 = 0;
    frame->r12 = 0;
    frame->r11 = 0;
    frame->r10 = 0;
    frame->r9 = 0;
    frame->r8 = 0;
    frame->rdi = 0;
    frame->rsi = 0;
    frame->rbp = 0;
    frame->rdx = 0;
    frame->rcx = 0;
    frame->rbx = 0;
    frame->rax = 0;

    frame->vector = 0;
    frame->rip =
        (uint64_t)(uintptr_t)x86_64_task_bootstrap_trampoline;
    frame->cs = GDT_KERNEL_CODE;
    frame->rflags = X86_64_PREEMPT_INITIAL_RFLAGS;

    state->frame_address = frame_address;
    state->valid = 1;

    return 0;
}

void x86_64_preempt_enable(void)
{
    preempt_enabled = 1;
}

void x86_64_preempt_disable(void)
{
    preempt_enabled = 0;
}

int x86_64_preempt_is_enabled(void)
{
    return preempt_enabled != 0;
}

static int x86_64_preempt_resolve_target(
    struct task *task,
    struct x86_64_resume_target *target
)
{
    if (task == NULL || target == NULL)
        return -1;

    target->kind = X86_64_RESUME_NONE;
    target->context = NULL;

    /*
     * The generic scheduler records the authoritative continuation
     * kind. The architecture layer alone resolves that authority
     * into an architecture-specific transfer target.
     */
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

int x86_64_preempt_handle_timer(
    struct irq_frame *frame,
    struct x86_64_resume_target *target
)
{
    struct task *current;
    struct task *next = NULL;

    if (frame == NULL || target == NULL)
        return -1;

    target->kind = X86_64_RESUME_NONE;
    target->frame = NULL;

    /*
     * With scheduler-driven preemption disabled, the interrupted
     * task's live interrupt frame remains the only continuation.
     */
    if (!preempt_enabled)
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    current = scheduler_get_current();

    if (current == NULL)
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    /*
     * The timer frame becomes the current task's authoritative
     * continuation before scheduler ownership is evaluated.
     */
    current->preempt_state.frame_address =
        (uintptr_t)frame;
    current->preempt_state.valid = 1;
    current->resume_authority = TASK_RESUME_INTERRUPT;

    /*
     * First inspect the scheduler candidate without changing
     * ownership. The generic scheduler remains responsible for
     * the actual READY/RUNNING transition.
     */
    struct task *candidate = scheduler_peek_next();

    if (candidate == NULL)
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    /*
     * A candidate must have a valid authoritative continuation
     * before it can become the next CPU owner.
     */
    struct x86_64_resume_target candidate_target;

    if (x86_64_preempt_resolve_target(
            candidate,
            &candidate_target) != 0)
    {
        /*
         * The candidate is not architecturally resumable.
         * Do not ask the generic scheduler to transfer ownership.
         */
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    /*
     * Perform the architecture-neutral scheduler ownership
     * transition using only the exact candidate whose
     * continuation was validated above. The architecture layer
     * already owns the fully resolved transfer target, so no
     * architecture state needs to be resolved after ownership
     * transfer.
     */
    int result = scheduler_preempt_current(candidate, &next);

    if (result < 0 || next == NULL)
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    /*
     * No-switch case: current retains the live interrupt frame.
     */
    if (next == current)
    {
        target->kind = X86_64_RESUME_INTERRUPT;
        target->frame = frame;
        return 0;
    }

    /*
     * `candidate_target` was resolved before scheduler ownership
     * transfer and therefore remains the authoritative typed
     * architecture handoff target.
     */
    *target = candidate_target;

    return 0;
}
