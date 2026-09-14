#include "preempt.h"

#include "context.h"
#include "../cpu/gdt.h"
#include "../interrupt/irq.h"

#include "../../../sched/task.h"

#include <stdint.h>
#include <stddef.h>

extern void x86_64_task_bootstrap_trampoline(void);

#define X86_64_PREEMPT_INITIAL_RFLAGS 0x202ULL

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
