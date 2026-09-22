#ifndef BATOS_X86_64_SCHED_DISPATCH_H
#define BATOS_X86_64_SCHED_DISPATCH_H

struct task;
struct x86_64_resume_target;

/*
 * Resolve the authoritative continuation of a task into the
 * architecture-specific transfer representation.
 *
 * This is a pure preflight operation:
 *   - no scheduler ownership changes
 *   - no task-state changes
 *   - no runqueue changes
 *   - no interrupt-state changes
 *
 * Both timer-driven preemption and ordinary scheduler dispatch
 * use this boundary so continuation authority is interpreted only
 * by the architecture layer.
 */
int x86_64_scheduler_resolve_target(
    struct task *task,
    struct x86_64_resume_target *target
);

/*
 * Perform the architecture-specific continuation handoff after
 * the generic scheduler has selected `next`.
 *
 * The generic scheduler owns task/runqueue/state transitions.
 * This boundary owns interpretation of the selected task's
 * continuation authority.
 *
 * CONTEXT authority:
 *   save current cooperative context
 *   restore next cooperative context
 *   enable interrupts
 *
 * INTERRUPT authority:
 *   save current cooperative context
 *   restore next interrupt-return frame
 *   iretq using the target frame's saved RFLAGS
 */
int x86_64_scheduler_dispatch(
    struct task *current,
    const struct x86_64_resume_target *target
);

#endif
