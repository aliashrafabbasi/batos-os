#include "preempt_authority_tests.h"

#include "../sched/task.h"
#include "../sched/scheduler.h"
#include "../sched/runqueue.h"
#include "../arch/x86_64/sched/preempt.h"
#include "../arch/x86_64/sched/dispatch.h"
#include "../arch/x86_64/interrupt/irq.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>

static void preempt_authority_test_entry(void *argument)
{
    (void)argument;
}

static void preempt_authority_test_fail(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void preempt_authority_cleanup(
    struct task *task_a,
    struct task *task_b
)
{
    /*
     * The test deliberately manipulates scheduler ownership without
     * entering the task context. Reset scheduler ownership first,
     * then release task-owned resources through the normal lifecycle
     * contract.
     */
    x86_64_preempt_disable();

    if (scheduler_init() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CLEANUP SCHEDULER: FAILED\n"
        );
    }

    if (task_a != NULL &&
        task_a->state == TASK_STATE_RUNNING)
    {
        if (task_transition(
                task_a,
                TASK_STATE_READY
            ) != 0)
        {
            preempt_authority_test_fail(
                "PREEMPTION-3 CLEANUP A TRANSITION: FAILED\n"
            );
        }
    }

    if (task_b != NULL &&
        task_b->state == TASK_STATE_RUNNING)
    {
        if (task_transition(
                task_b,
                TASK_STATE_READY
            ) != 0)
        {
            preempt_authority_test_fail(
                "PREEMPTION-3 CLEANUP B TRANSITION: FAILED\n"
            );
        }
    }

    /*
     * Scheduler ownership and architecture continuation ownership
     * are separate lifecycle resources. The test has deliberately
     * installed interrupt authority on these tasks, so cleanup must
     * explicitly release that authority before final destruction.
     */
    if (task_a != NULL)
    {
        task_a->resume_authority = TASK_RESUME_NONE;
        task_a->preempt_state.valid = 0;
        task_a->preempt_state.frame_address = 0;
    }

    if (task_b != NULL)
    {
        task_b->resume_authority = TASK_RESUME_NONE;
        task_b->preempt_state.valid = 0;
        task_b->preempt_state.frame_address = 0;
    }

    if (task_a != NULL &&
        task_a->state != TASK_STATE_TERMINATED)
    {
        if (task_destroy(task_a) != 0)
        {
            preempt_authority_test_fail(
                "PREEMPTION-3 CLEANUP A DESTROY: FAILED\n"
            );
        }
    }

    if (task_b != NULL &&
        task_b->state != TASK_STATE_TERMINATED)
    {
        if (task_destroy(task_b) != 0)
        {
            preempt_authority_test_fail(
                "PREEMPTION-3 CLEANUP B DESTROY: FAILED\n"
            );
        }
    }
}

static void preempt_authority_test_no_candidate(void)
{
    struct task task_a = {0};
    struct irq_frame frame = {0};
    struct x86_64_resume_target target = {0};

    uint64_t pml4 = vmm_get_pml4();

    if (pml4 == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE ADDRESS SPACE: FAILED\n"
        );
    }

    if (scheduler_init() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE SCHEDULER: FAILED\n"
        );
    }

    if (task_create(
            &task_a,
            250,
            pml4,
            preempt_authority_test_entry,
            NULL
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE TASK CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&task_a) != 0 ||
        scheduler_start() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE START: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_a ||
        task_a.state != TASK_STATE_RUNNING ||
        runqueue_count() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE INITIAL STATE: FAILED\n"
        );
    }

    __asm__ volatile ("cli" ::: "memory");

    x86_64_preempt_enable();

    if (x86_64_preempt_handle_timer(
            &frame,
            &target
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE HANDLER: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_a ||
        task_a.state != TASK_STATE_RUNNING ||
        runqueue_count() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE OWNERSHIP: FAILED\n"
        );
    }

    if (task_a.resume_authority !=
        TASK_RESUME_INTERRUPT)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE AUTHORITY: FAILED\n"
        );
    }

    if (task_a.preempt_state.valid == 0 ||
        task_a.preempt_state.frame_address !=
            (uintptr_t)&frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE FRAME BINDING: FAILED\n"
        );
    }

    if (target.kind != X86_64_RESUME_INTERRUPT ||
        target.frame != &frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 NO-CANDIDATE TARGET: FAILED\n"
        );
    }

    serial_write_string(
        "PREEMPTION-3 NO-CANDIDATE: VERIFIED\n"
    );

    preempt_authority_cleanup(
        &task_a,
        NULL
    );
}

static void preempt_authority_test_invalid_candidate(void)
{
    struct task task_a = {0};
    struct task task_b = {0};
    struct irq_frame frame = {0};
    struct x86_64_resume_target target = {0};

    uint64_t pml4 = vmm_get_pml4();

    if (pml4 == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE ADDRESS SPACE: FAILED\n"
        );
    }

    if (scheduler_init() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE SCHEDULER: FAILED\n"
        );
    }

    if (task_create(
            &task_a,
            251,
            pml4,
            preempt_authority_test_entry,
            NULL
        ) != 0 ||
        task_create(
            &task_b,
            252,
            pml4,
            preempt_authority_test_entry,
            NULL
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE TASK CREATE: FAILED\n"
        );
    }

    /*
     * B is deliberately made architecturally non-resumable.
     * It remains a valid READY scheduler candidate, but its
     * interrupt continuation is invalid.
     */
    task_b.resume_authority = TASK_RESUME_INTERRUPT;
    task_b.preempt_state.valid = 0;
    task_b.preempt_state.frame_address = 0;

    if (scheduler_add(&task_a) != 0 ||
        scheduler_add(&task_b) != 0 ||
        scheduler_start() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE START: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_a ||
        task_a.state != TASK_STATE_RUNNING ||
        task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&task_b) ||
        runqueue_count() != 1)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE INITIAL STATE: FAILED\n"
        );
    }

    uint64_t dispatch_before =
        scheduler_get_dispatch_count();

    __asm__ volatile ("cli" ::: "memory");

    x86_64_preempt_enable();

    if (x86_64_preempt_handle_timer(
            &frame,
            &target
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE HANDLER: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_a ||
        task_a.state != TASK_STATE_RUNNING ||
        task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&task_b) ||
        runqueue_count() != 1)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE OWNERSHIP: FAILED\n"
        );
    }

    if (scheduler_get_dispatch_count() !=
        dispatch_before)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE DISPATCH: FAILED\n"
        );
    }

    if (task_a.resume_authority !=
        TASK_RESUME_INTERRUPT)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE AUTHORITY: FAILED\n"
        );
    }

    if (task_a.preempt_state.valid == 0 ||
        task_a.preempt_state.frame_address !=
            (uintptr_t)&frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE FRAME BINDING: FAILED\n"
        );
    }

    if (target.kind != X86_64_RESUME_INTERRUPT ||
        target.frame != &frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INVALID-CANDIDATE TARGET: FAILED\n"
        );
    }

    serial_write_string(
        "PREEMPTION-3 INVALID-CANDIDATE: VERIFIED\n"
    );

    preempt_authority_cleanup(
        &task_a,
        &task_b
    );
}

static void preempt_authority_test_context_candidate(void)
{
    struct task task_a = {0};
    struct task task_b = {0};
    struct irq_frame frame = {0};
    struct x86_64_resume_target target = {0};

    uint64_t pml4 = vmm_get_pml4();

    if (pml4 == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE ADDRESS SPACE: FAILED\n"
        );
    }

    if (scheduler_init() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE SCHEDULER: FAILED\n"
        );
    }

    if (task_create(
            &task_a,
            253,
            pml4,
            preempt_authority_test_entry,
            NULL
        ) != 0 ||
        task_create(
            &task_b,
            254,
            pml4,
            preempt_authority_test_entry,
            NULL
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE TASK CREATE: FAILED\n"
        );
    }

    /*
     * task_create() deliberately stages a usable interrupt-return
     * frame for first-run support, while CONTEXT remains the
     * authoritative continuation. The preemption resolver must
     * therefore select B's cooperative context, not its staged
     * interrupt frame.
     */
    if (task_b.resume_authority != TASK_RESUME_CONTEXT ||
        task_b.preempt_state.valid == 0 ||
        task_b.preempt_state.frame_address == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE AUTHORITY SETUP: FAILED\n"
        );
    }

    if (scheduler_add(&task_a) != 0 ||
        scheduler_add(&task_b) != 0 ||
        scheduler_start() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE START: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_a ||
        task_a.state != TASK_STATE_RUNNING ||
        task_b.state != TASK_STATE_READY ||
        !runqueue_contains(&task_b) ||
        runqueue_count() != 1)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE INITIAL STATE: FAILED\n"
        );
    }

    uint64_t dispatch_before =
        scheduler_get_dispatch_count();

    __asm__ volatile ("cli" ::: "memory");

    x86_64_preempt_enable();

    if (x86_64_preempt_handle_timer(
            &frame,
            &target
        ) != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE HANDLER: FAILED\n"
        );
    }

    if (scheduler_get_current() != &task_b ||
        task_a.state != TASK_STATE_READY ||
        task_b.state != TASK_STATE_RUNNING ||
        runqueue_contains(&task_a) == 0 ||
        runqueue_count() != 1)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE OWNERSHIP: FAILED\n"
        );
    }

    if (scheduler_get_dispatch_count() !=
        dispatch_before + 1)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE DISPATCH: FAILED\n"
        );
    }

    /*
     * A's live interrupt frame became authoritative when the timer
     * interrupted it. B retains CONTEXT authority because its
     * cooperative continuation was selected for resumption.
     */
    if (task_a.resume_authority != TASK_RESUME_INTERRUPT ||
        task_a.preempt_state.valid == 0 ||
        task_a.preempt_state.frame_address !=
            (uintptr_t)&frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE CURRENT AUTHORITY: FAILED\n"
        );
    }

    if (task_b.resume_authority != TASK_RESUME_CONTEXT)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE TARGET AUTHORITY: FAILED\n"
        );
    }

    if (target.kind != X86_64_RESUME_CONTEXT ||
        target.context != &task_b.context)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 CONTEXT-CANDIDATE TARGET: FAILED\n"
        );
    }

    serial_write_string(
        "PREEMPTION-3 CONTEXT-CANDIDATE: VERIFIED\n"
    );

    preempt_authority_cleanup(
        &task_a,
        &task_b
    );
}


static void preempt_authority_test_resolver_contract(void)
{
    struct task task = {0};
    struct irq_frame frame = {0};
    struct x86_64_resume_target target = {0};

    /*
     * Resolver inputs are architecture-layer contracts. Invalid
     * pointers must be rejected without producing a transfer target.
     */
    if (x86_64_scheduler_resolve_target(
            NULL,
            &target
        ) == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER NULL TASK: FAILED\n"
        );
    }

    if (x86_64_scheduler_resolve_target(
            &task,
            NULL
        ) == 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER NULL TARGET: FAILED\n"
        );
    }

    /*
     * NONE authority is not resumable.
     */
    task.resume_authority = TASK_RESUME_NONE;
    target.kind = X86_64_RESUME_CONTEXT;
    target.context = &task.context;

    if (x86_64_scheduler_resolve_target(
            &task,
            &target
        ) == 0 ||
        target.kind != X86_64_RESUME_NONE ||
        target.context != NULL)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER NONE: FAILED\n"
        );
    }

    /*
     * INTERRUPT authority without a valid architecture-owned frame
     * must be rejected.
     */
    task.resume_authority = TASK_RESUME_INTERRUPT;
    task.preempt_state.frame_address = 0;
    task.preempt_state.valid = 0;

    if (x86_64_scheduler_resolve_target(
            &task,
            &target
        ) == 0 ||
        target.kind != X86_64_RESUME_NONE ||
        target.frame != NULL)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER INVALID INTERRUPT: FAILED\n"
        );
    }

    /*
     * CONTEXT authority must resolve exclusively to the task's
     * cooperative continuation, even when an interrupt frame is
     * simultaneously staged.
     */
    task.resume_authority = TASK_RESUME_CONTEXT;
    task.preempt_state.frame_address =
        (uintptr_t)&frame;
    task.preempt_state.valid = 1;

    enum task_state state_before = task.state;
    enum task_resume_authority authority_before =
        task.resume_authority;
    uintptr_t frame_address_before =
        task.preempt_state.frame_address;
    uint64_t frame_valid_before =
        task.preempt_state.valid;

    if (x86_64_scheduler_resolve_target(
            &task,
            &target
        ) != 0 ||
        target.kind != X86_64_RESUME_CONTEXT ||
        target.context != &task.context)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER CONTEXT: FAILED\n"
        );
    }

    /*
     * Resolver purity: no generic scheduler/task ownership state
     * may change during architecture target resolution.
     */
    if (task.state != state_before ||
        task.resume_authority != authority_before ||
        task.preempt_state.frame_address !=
            frame_address_before ||
        task.preempt_state.valid != frame_valid_before)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER CONTEXT PURITY: FAILED\n"
        );
    }

    /*
     * INTERRUPT authority must resolve to the architecture-owned
     * interrupt-return frame.
     */
    task.resume_authority = TASK_RESUME_INTERRUPT;

    if (x86_64_scheduler_resolve_target(
            &task,
            &target
        ) != 0 ||
        target.kind != X86_64_RESUME_INTERRUPT ||
        target.frame != &frame)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER INTERRUPT: FAILED\n"
        );
    }

    if (task.state != state_before ||
        task.resume_authority != TASK_RESUME_INTERRUPT ||
        task.preempt_state.frame_address !=
            frame_address_before ||
        task.preempt_state.valid != frame_valid_before)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 RESOLVER INTERRUPT PURITY: FAILED\n"
        );
    }

    serial_write_string(
        "PREEMPTION-3 RESOLVER CONTRACT: VERIFIED\n"
    );
}

void preempt_authority_tests_run(void)
{
    serial_write_string(
        "\nPREEMPTION-3 CONTINUATION AUTHORITY TEST\n"
    );

    if (x86_64_preempt_is_enabled() != 0)
    {
        preempt_authority_test_fail(
            "PREEMPTION-3 INITIAL PREEMPT STATE: INVALID\n"
        );
    }

    preempt_authority_test_no_candidate();
    preempt_authority_test_invalid_candidate();
    preempt_authority_test_context_candidate();
    preempt_authority_test_resolver_contract();

    x86_64_preempt_disable();

    serial_write_string(
        "PREEMPTION-3 CONTINUATION AUTHORITY: VERIFIED\n"
    );
}
