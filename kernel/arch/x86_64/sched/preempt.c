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

uintptr_t x86_64_preempt_handle_timer(
    struct irq_frame *frame
)
{
    struct task *current;
    struct task *next = NULL;

    if (frame == NULL)
        return 0;

    /*
     * The LAPIC timer remains a valid clock-event source even
     * when scheduler-driven preemption is not active.
     *
     * The interrupt frame is therefore always the safe
     * continuation until an explicit scheduler runtime
     * activation enables preemption.
     */
    if (!preempt_enabled)
        return (uintptr_t)frame;

    current = scheduler_get_current();

    if (current == NULL)
        return (uintptr_t)frame;

    /*
     * The timer frame is the live architectural continuation
     * of the currently running task. Bind it before asking the
     * generic scheduler to transfer ownership.
     */
    current->preempt_state.frame_address =
        (uintptr_t)frame;
    current->preempt_state.valid = 1;

    /*
     * The live timer frame is now the current task's
     * authoritative continuation. This transition belongs
     * to the architecture boundary because only this layer
     * owns and interprets the interrupt-return representation.
     *
     * A fresh task remains CONTEXT-authoritative: its
     * synthetic preemptive frame is only a first-run adapter.
     */
    current->resume_authority =
        TASK_RESUME_INTERRUPT;

    /*
     * Scheduler policy/state transition is architecture-neutral.
     * It returns the task that should own the next CPU context.
     */
    int result = scheduler_preempt_current(&next);

    if (result < 0 || next == NULL)
    {
        /*
         * Keep the current live frame as the safe continuation.
         * The scheduler contract guarantees that an error leaves
         * the current task running.
         */
        return (uintptr_t)frame;
    }

    /*
     * No-switch case: the current task remains the owner of
     * this exact interrupt frame.
     */
    if (next == current)
        return (uintptr_t)frame;

    /*
     * Switch case: the selected READY task owns an architecture-
     * valid resumable frame by the task lifecycle contract.
     *
     * That frame is either the synthetic first-run frame or a
     * previously saved live interrupt frame.
     */
    return next->preempt_state.frame_address;
}
