#ifndef BATOS_X86_64_PREEMPT_H
#define BATOS_X86_64_PREEMPT_H

#include <stdint.h>
#include <stddef.h>

struct task;
struct irq_frame;

/*
 * Architecture-owned handle for a task's resumable
 * interrupt-return state.
 *
 * Generic task/scheduler code must not interpret the
 * saved frame directly.
 */
struct x86_64_preempt_state
{
    uintptr_t frame_address;
    uint64_t valid;
};

_Static_assert(
    offsetof(struct x86_64_preempt_state, frame_address) == 0,
    "preempt state frame address offset mismatch"
);

_Static_assert(
    offsetof(struct x86_64_preempt_state, valid) == 8,
    "preempt state valid offset mismatch"
);

_Static_assert(
    sizeof(struct x86_64_preempt_state) == 16,
    "preempt state size mismatch"
);

/*
 * Prepare the architecture-owned interrupt-return state
 * for a task that has not executed yet.
 *
 * The synthetic frame is placed immediately below the
 * existing cooperative bootstrap context. The existing
 * task bootstrap ABI therefore remains unchanged.
 */
int x86_64_preempt_prepare_first_run(
    struct task *task,
    struct x86_64_preempt_state *state
);

/*
 * Validate an architecture-owned preemptive state.
 *
 * Generic scheduler/task code must not inspect the saved
 * interrupt frame directly.
 */
int x86_64_preempt_state_is_valid(
    const struct x86_64_preempt_state *state
);

/*
 * Identifies which architecture-owned continuation must
 * receive control after a scheduler-driven preemption.
 *
 * The transfer kind is intentionally explicit. A raw address
 * is insufficient because an interrupt-return frame and a
 * cooperative CPU context require different restoration
 * mechanisms.
 */
enum x86_64_resume_kind
{
    X86_64_RESUME_NONE = 0,
    X86_64_RESUME_CONTEXT,
    X86_64_RESUME_INTERRUPT
};

struct x86_64_resume_target
{
    enum x86_64_resume_kind kind;

    union
    {
        const struct x86_64_context *context;
        struct irq_frame *frame;
    };
};

_Static_assert(
    offsetof(struct x86_64_resume_target, kind) == 0,
    "resume target kind offset mismatch"
);

_Static_assert(
    offsetof(struct x86_64_resume_target, context) == 8,
    "resume target pointer offset mismatch"
);

_Static_assert(
    sizeof(struct x86_64_resume_target) == 16,
    "resume target size mismatch"
);

/*
 * Handle the architecture boundary for a LAPIC timer
 * preemption event.
 *
 * The architecture layer owns the live interrupt frame,
 * binds it to the currently running task, asks the generic
 * scheduler for the next task, and returns an explicit
 * architecture transfer target.
 *
 * Generic scheduler code never interprets the frame or
 * context representation.
 */
int x86_64_preempt_handle_timer(
    struct irq_frame *frame,
    struct x86_64_resume_target *target
);

/*
 * Restore a cooperative continuation selected by scheduler-driven
 * timer preemption and resume it with interrupts enabled.
 */
__attribute__((noreturn))
void x86_64_preempt_restore_context_and_resume(
    const struct x86_64_context *next
);

/*
 * Control whether LAPIC timer interrupts may perform
 * scheduler-driven preemption.
 *
 * Preemption is disabled by default. The clock-event source
 * remains independently operational while scheduler runtime
 * activation has not yet occurred.
 */
void x86_64_preempt_enable(void);
void x86_64_preempt_disable(void);
int x86_64_preempt_is_enabled(void);

#endif
