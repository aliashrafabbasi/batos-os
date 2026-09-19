#include "preempt_authority_tests.h"

#include "../sched/task.h"
#include "../sched/scheduler.h"
#include "../sched/runqueue.h"
#include "../arch/x86_64/sched/preempt.h"
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

    x86_64_preempt_disable();

    serial_write_string(
        "PREEMPTION-3 CONTINUATION AUTHORITY: VERIFIED\n"
    );
}
