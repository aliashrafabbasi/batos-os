#include "task_tests.h"

#include "../sched/task.h"
#include "../sched/scheduler.h"
#include "../sched/runqueue.h"
#include "../sched/task_registry.h"
#include "../arch/x86_64/sched/context.h"
#include "../mm/pmm/pmm.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static struct task task_execution_a;
static struct task task_execution_b;

/*
 * Test-only cooperative cleanup continuation.
 *
 * This continuation is deliberately not created through
 * task_create(). It exists only to provide a legitimate
 * scheduler-owned successor so Task B can leave RUNNING
 * state before the harness destroys it.
 */
static struct task task_execution_cleanup_task;
static uint8_t task_execution_cleanup_stack[4096];

static struct x86_64_context task_execution_harness_context;

static volatile uint64_t task_execution_a_reached = 0;
static volatile uint64_t task_execution_b_reached = 0;
static volatile uint64_t task_execution_a_terminated = 0;

static uint64_t task_execution_expected_argument =
    0x4241544F535F5441ULL;

static void task_execution_test_a(void *argument);
static void task_execution_test_b(void *argument);
static void task_execution_cleanup_entry(void *argument);

static volatile uint64_t task_execution_destroy_rejected = 0;

static int task_execution_exit_handler(struct task *task);

extern void x86_64_task_bootstrap_trampoline(void);

static void task_test_entry(void *argument)
{
    (void)argument;
}

static void task_test_fail(const char *message)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

static void task_transition_contract_test(void)
{
    /*
     * SCHED-2 lifecycle contract matrix.
     *
     * The matrix is indexed as:
     *
     *   [current_state][requested_state]
     *
     * 1 = legal transition
     * 0 = rejected transition
     */
    static const uint8_t allowed[
        6
    ][
        6
    ] =
    {
        /* NEW        */
        { 0, 1, 0, 0, 0, 0 },

        /* READY      */
        { 0, 0, 1, 0, 0, 0 },

        /* RUNNING    */
        { 0, 1, 0, 1, 1, 1 },

        /* BLOCKED    */
        { 0, 1, 0, 0, 0, 0 },

        /* SLEEPING   */
        { 0, 1, 0, 0, 0, 0 },

        /* TERMINATED */
        { 0, 0, 0, 0, 0, 0 }
    };

    for (uint64_t from = TASK_STATE_NEW;
         from <= TASK_STATE_TERMINATED;
         from++)
    {
        for (uint64_t to = TASK_STATE_NEW;
             to <= TASK_STATE_TERMINATED;
             to++)
        {
            struct task task = {0};

            task.state =
                (enum task_state)from;

            int result =
                task_transition(
                    &task,
                    (enum task_state)to
                );

            if (allowed[from][to])
            {
                if (result != 0 ||
                    task.state !=
                        (enum task_state)to)
                {
                    task_test_fail(
                        "TASK LIFECYCLE TRANSITION: LEGAL REJECTED\n"
                    );
                }
            }
            else
            {
                if (result == 0 ||
                    task.state !=
                        (enum task_state)from)
                {
                    task_test_fail(
                        "TASK LIFECYCLE TRANSITION: INVALID ACCEPTED\n"
                    );
                }
            }
        }
    }

    serial_write_string(
        "TASK LIFECYCLE TRANSITIONS: VERIFIED\n"
    );
}

void task_tests_run(void)
{
    serial_write_string(
        "\nTASK FOUNDATION TEST\n"
    );

    task_transition_contract_test();

    struct task task = {0};

    uint64_t pml4 =
        vmm_get_pml4();

    if (pml4 == 0)
    {
        task_test_fail(
            "TASK TEST: NO ADDRESS SPACE\n"
        );
    }

    uint64_t free_before =
        pmm_get_free_frames();

    if (task_create(
            &task,
            1,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK CREATE: FAILED\n"
        );
    }

    uint64_t free_after_create =
        pmm_get_free_frames();

    if (task.state != TASK_STATE_READY ||
        task.resume_authority != TASK_RESUME_CONTEXT ||
        task.kernel_stack_base == 0 ||
        task.kernel_stack_top <=
            task.kernel_stack_base ||
        task.context.rsp !=
            task.kernel_stack_top -
            3ULL * sizeof(uint64_t) ||
        task.context.rip == 0 ||
        free_before - free_after_create !=
            TASK_KERNEL_STACK_PAGE_COUNT)
    {
        task_test_fail(
            "TASK CREATE: INVALID\n"
        );
    }

    /*
     * Verify the initial bootstrap context ABI:
     *
     *   RSP % 16 == 8
     *   [RSP + 0] == 0
     *   [RSP + 8] == task
     *   RIP == bootstrap trampoline
     */
    uint64_t bootstrap_rsp = task.context.rsp;

    if ((bootstrap_rsp & 0xFULL) != 8 ||
        *(uint64_t *)(uintptr_t)bootstrap_rsp != 0 ||
        *(uint64_t *)(uintptr_t)(bootstrap_rsp +
                                 sizeof(uint64_t)) !=
            (uint64_t)(uintptr_t)&task ||
        task.context.rip !=
            (uint64_t)(uintptr_t)x86_64_task_bootstrap_trampoline)
    {
        task_test_fail(
            "TASK BOOTSTRAP STACK: INVALID\n"
        );
    }

    serial_write_string(
        "TASK BOOTSTRAP STACK: VERIFIED\n"
    );

    /*
     * Verify every stack page has a VMM translation.
     */
    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        uint64_t translated = 0;

        uint64_t virtual_address =
            task.kernel_stack_base +
            page * VMM_PAGE_SIZE;

        if (vmm_translate(
                pml4,
                virtual_address,
                &translated
            ) != 0)
        {
            task_test_fail(
                "TASK STACK MAP: FAILED\n"
            );
        }

        if (translated !=
            task.kernel_stack_pages[page])
        {
            task_test_fail(
                "TASK STACK OWNERSHIP: INVALID\n"
            );
        }
    }

    /*
     * The guard page immediately below the stack must remain
     * unmapped.
     */
    uint64_t guard_physical = 0;

    if (vmm_translate(
            pml4,
            task.kernel_stack_base -
                TASK_KERNEL_STACK_GUARD_SIZE,
            &guard_physical
        ) == 0)
    {
        task_test_fail(
            "TASK STACK GUARD: MAPPED\n"
        );
    }

    /*
     * Stack teardown must reject an ownership mismatch before
     * modifying any mapping or releasing any physical frame.
     */
    uint64_t saved_stack_physical =
        task.kernel_stack_pages[1];

    uint64_t free_before_invalid_destroy =
        pmm_get_free_frames();

    task.kernel_stack_pages[1] =
        saved_stack_physical ^ VMM_PAGE_SIZE;

    if (task_destroy(&task) == 0)
    {
        task_test_fail(
            "TASK STACK TEARDOWN: INVALID OWNERSHIP ACCEPTED\n"
        );
    }

    if (task.state != TASK_STATE_READY ||
        task.kernel_stack_base == 0 ||
        task.kernel_stack_top == 0 ||
        pmm_get_free_frames() !=
            free_before_invalid_destroy)
    {
        task_test_fail(
            "TASK STACK TEARDOWN: STATE MUTATED\n"
        );
    }

    /*
     * The failed destroy must leave every mapping untouched.
     * Page 1 is compared against the original physical frame,
     * while the other pages must still match their ownership
     * metadata.
     */
    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        uint64_t translated = 0;

        uint64_t virtual_address =
            task.kernel_stack_base +
            page * VMM_PAGE_SIZE;

        if (vmm_translate(
                pml4,
                virtual_address,
                &translated
            ) != 0)
        {
            task_test_fail(
                "TASK STACK TEARDOWN: MAPPING LOST\n"
            );
        }

        if (page == 1)
        {
            if (translated != saved_stack_physical)
            {
                task_test_fail(
                    "TASK STACK TEARDOWN: PARTIAL UNMAP\n"
                );
            }
        }
        else if (translated !=
                 task.kernel_stack_pages[page])
        {
            task_test_fail(
                "TASK STACK TEARDOWN: OWNERSHIP CHANGED\n"
            );
        }
    }

    task.kernel_stack_pages[1] =
        saved_stack_physical;

    serial_write_string(
        "TASK STACK TEARDOWN ATOMICITY: VERIFIED\n"
    );

    if (task_destroy(&task) != 0)
    {
        task_test_fail(
            "TASK DESTROY: FAILED\n"
        );
    }

    if (task.resume_authority != TASK_RESUME_NONE)
    {
        task_test_fail(
            "TASK RESUME AUTHORITY CLEANUP: FAILED\n"
        );
    }

    uint64_t free_after_destroy =
        pmm_get_free_frames();

    if (task.state != TASK_STATE_TERMINATED ||
        free_after_destroy != free_before)
    {
        task_test_fail(
            "TASK STACK RELEASE: FAILED\n"
        );
    }

    serial_write_string(
        "TASK OBJECT: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK MAPPING: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK GUARD: VERIFIED\n"
    );

    serial_write_string(
        "TASK STACK OWNERSHIP: VERIFIED\n"
    );

    /*
     * Lifecycle ownership contract:
     *
     * A READY task may be destroyed while it has no external
     * scheduler membership. Once admitted to the runqueue,
     * runnable ownership must be released before task-owned
     * resources are destroyed.
     */
    struct task lifecycle_task = {0};

    if (scheduler_init() != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SCHEDULER INIT: FAILED\n"
        );
    }

    if (task_create(
            &lifecycle_task,
            4,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&lifecycle_task) != 0 ||
        !runqueue_contains(&lifecycle_task))
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE ADMISSION: FAILED\n"
        );
    }

    /*
     * Runnable ownership must block destruction. A failed
     * destroy attempt must leave the task completely intact.
     */
    uint64_t lifecycle_stack_base =
        lifecycle_task.kernel_stack_base;

    uint64_t lifecycle_stack_top =
        lifecycle_task.kernel_stack_top;

    enum task_state lifecycle_state =
        lifecycle_task.state;

    enum task_resume_authority lifecycle_resume_authority =
        lifecycle_task.resume_authority;

    if (task_destroy(&lifecycle_task) == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE DESTROY: ACCEPTED\n"
        );
    }

    if (!runqueue_contains(&lifecycle_task) ||
        lifecycle_task.kernel_stack_base !=
            lifecycle_stack_base ||
        lifecycle_task.kernel_stack_top !=
            lifecycle_stack_top ||
        lifecycle_task.state !=
            lifecycle_state ||
        lifecycle_task.resume_authority !=
            lifecycle_resume_authority)
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE DESTROY: CORRUPTED\n"
        );
    }

    if (runqueue_remove(&lifecycle_task) != 0 ||
        runqueue_contains(&lifecycle_task))
    {
        task_test_fail(
            "TASK LIFECYCLE RUNQUEUE RELEASE: FAILED\n"
        );
    }

    if (task_destroy(&lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE DESTROY: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE RUNQUEUE OWNERSHIP: VERIFIED\n"
    );

    /*
     * Registry membership is independently owned. It must be
     * released before final task resource destruction.
     */
    struct task registry_lifecycle_task = {0};

    if (task_create(
            &registry_lifecycle_task,
            5,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY CREATE: FAILED\n"
        );
    }

    if (task_registry_init() != 0 ||
        task_registry_register(&registry_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY ADMISSION: FAILED\n"
        );
    }

    if (task_registry_find(
            registry_lifecycle_task.id
        ) != &registry_lifecycle_task)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY OWNERSHIP: FAILED\n"
        );
    }

    /*
     * Registry ownership must independently block destruction.
     * The failed destroy must not release task-owned resources.
     */
    uint64_t registry_stack_base =
        registry_lifecycle_task.kernel_stack_base;

    uint64_t registry_stack_top =
        registry_lifecycle_task.kernel_stack_top;

    enum task_state registry_state =
        registry_lifecycle_task.state;

    enum task_resume_authority registry_resume_authority =
        registry_lifecycle_task.resume_authority;

    if (task_destroy(&registry_lifecycle_task) == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY DESTROY: ACCEPTED\n"
        );
    }

    if (!task_registry_contains(
            &registry_lifecycle_task
        ) ||
        registry_lifecycle_task.kernel_stack_base !=
            registry_stack_base ||
        registry_lifecycle_task.kernel_stack_top !=
            registry_stack_top ||
        registry_lifecycle_task.state !=
            registry_state ||
        registry_lifecycle_task.resume_authority !=
            registry_resume_authority)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY DESTROY: CORRUPTED\n"
        );
    }

    if (task_registry_unregister(
            &registry_lifecycle_task
        ) != 0 ||
        task_registry_find(
            registry_lifecycle_task.id
        ) != NULL)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY RELEASE: FAILED\n"
        );
    }

    if (task_destroy(&registry_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE REGISTRY DESTROY: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE REGISTRY OWNERSHIP: VERIFIED\n"
    );

    /*
     * BLOCKED and SLEEPING tasks retain execution ownership
     * outside the runnable queue. Final destruction must wait
     * until those ownership states are released.
     */
    struct task blocked_lifecycle_task = {0};
    struct task sleeping_lifecycle_task = {0};

    if (task_create(
            &blocked_lifecycle_task,
            6,
            pml4,
            task_test_entry,
            NULL
        ) != 0 ||
        task_transition(
            &blocked_lifecycle_task,
            TASK_STATE_RUNNING
        ) != 0 ||
        task_transition(
            &blocked_lifecycle_task,
            TASK_STATE_BLOCKED
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE BLOCKED SETUP: FAILED\n"
        );
    }

    if (task_destroy(&blocked_lifecycle_task) == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE BLOCKED DESTROY: ACCEPTED\n"
        );
    }

    if (blocked_lifecycle_task.state !=
            TASK_STATE_BLOCKED ||
        blocked_lifecycle_task.kernel_stack_base == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE BLOCKED DESTROY: CORRUPTED\n"
        );
    }

    if (task_transition(
            &blocked_lifecycle_task,
            TASK_STATE_READY
        ) != 0 ||
        task_destroy(&blocked_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE BLOCKED RELEASE: FAILED\n"
        );
    }

    if (task_create(
            &sleeping_lifecycle_task,
            7,
            pml4,
            task_test_entry,
            NULL
        ) != 0 ||
        task_transition(
            &sleeping_lifecycle_task,
            TASK_STATE_RUNNING
        ) != 0 ||
        task_transition(
            &sleeping_lifecycle_task,
            TASK_STATE_SLEEPING
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SLEEPING SETUP: FAILED\n"
        );
    }

    if (task_destroy(&sleeping_lifecycle_task) == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SLEEPING DESTROY: ACCEPTED\n"
        );
    }

    if (sleeping_lifecycle_task.state !=
            TASK_STATE_SLEEPING ||
        sleeping_lifecycle_task.kernel_stack_base == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SLEEPING DESTROY: CORRUPTED\n"
        );
    }

    if (task_transition(
            &sleeping_lifecycle_task,
            TASK_STATE_READY
        ) != 0 ||
        task_destroy(&sleeping_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE SLEEPING RELEASE: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE BLOCKED/SLEEPING OWNERSHIP: VERIFIED\n"
    );

    /*
     * An architecture-owned interrupt continuation must block
     * final destruction until that continuation authority is
     * explicitly released.
     */
    struct task interrupt_lifecycle_task = {0};

    if (task_create(
            &interrupt_lifecycle_task,
            8,
            pml4,
            task_test_entry,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE INTERRUPT CREATE: FAILED\n"
        );
    }

    uint64_t interrupt_stack_base =
        interrupt_lifecycle_task.kernel_stack_base;

    uint64_t interrupt_stack_top =
        interrupt_lifecycle_task.kernel_stack_top;

    interrupt_lifecycle_task.resume_authority =
        TASK_RESUME_INTERRUPT;

    if (task_destroy(&interrupt_lifecycle_task) == 0)
    {
        task_test_fail(
            "TASK LIFECYCLE INTERRUPT DESTROY: ACCEPTED\n"
        );
    }

    if (interrupt_lifecycle_task.state !=
            TASK_STATE_READY ||
        interrupt_lifecycle_task.kernel_stack_base !=
            interrupt_stack_base ||
        interrupt_lifecycle_task.kernel_stack_top !=
            interrupt_stack_top ||
        interrupt_lifecycle_task.resume_authority !=
            TASK_RESUME_INTERRUPT)
    {
        task_test_fail(
            "TASK LIFECYCLE INTERRUPT DESTROY: CORRUPTED\n"
        );
    }

    interrupt_lifecycle_task.resume_authority =
        TASK_RESUME_CONTEXT;

    if (task_destroy(&interrupt_lifecycle_task) != 0)
    {
        task_test_fail(
            "TASK LIFECYCLE INTERRUPT RELEASE: FAILED\n"
        );
    }

    serial_write_string(
        "TASK LIFECYCLE INTERRUPT AUTHORITY: VERIFIED\n"
    );

    /*
     * Real task execution/termination verification.
     *
     * Task A executes its entry, verifies its argument, exits,
     * and the scheduler transfers control to Task B without
     * re-queueing the terminated task.
     */

    if (task_create(
            &task_execution_a,
            2,
            pml4,
            task_execution_test_a,
            (void *)(uintptr_t)
                task_execution_expected_argument
        ) != 0 ||
        task_create(
            &task_execution_b,
            3,
            pml4,
            task_execution_test_b,
            NULL
        ) != 0)
    {
        task_test_fail(
            "TASK EXECUTION CREATE: FAILED\n"
        );
    }

    if (scheduler_add(&task_execution_a) != 0 ||
        scheduler_add(&task_execution_b) != 0 ||
        scheduler_start() != 0)
    {
        task_test_fail(
            "TASK EXECUTION ADMISSION: FAILED\n"
        );
    }

    if (task_set_exit_handler(
            task_execution_exit_handler
        ) != 0)
    {
        task_test_fail(
            "TASK EXIT HANDLER INSTALL: FAILED\n"
        );
    }

    x86_64_context_switch(
        &task_execution_harness_context,
        &task_execution_a.context
    );

    /*
     * Task B transferred control back to the harness.
     * Restore the normal scheduler exit handler before
     * the test performs further lifecycle operations.
     */
    if (task_set_exit_handler(
            scheduler_exit_current
        ) != 0)
    {
        task_test_fail(
            "TASK EXIT HANDLER RESTORE: FAILED\n"
        );
    }

    if (task_execution_a_reached != 1 ||
        task_execution_b_reached != 1 ||
        task_execution_a_terminated != 1 ||
        task_execution_destroy_rejected != 1)
    {
        task_test_fail(
            "TASK EXECUTION: FAILED\n"
        );
    }

    if (task_execution_a.state !=
            TASK_STATE_TERMINATED ||
        task_execution_b.state !=
            TASK_STATE_RUNNING ||
        scheduler_get_current() !=
            &task_execution_b)
    {
        task_test_fail(
            "TASK TERMINATION STATE: FAILED\n"
        );
    }

    if (scheduler_get_dispatch_count() != 1)
    {
        task_test_fail(
            "TASK TERMINATION DISPATCH: FAILED\n"
        );
    }

    /*
     * The primary execution contract has now been observed:
     *
     *   A = TERMINATED
     *   B = RUNNING
     *   scheduler_current = B
     *
     * Re-enter B's saved cooperative continuation so B can perform
     * the scheduler-owned transition out of RUNNING state.
     */
    x86_64_context_switch(
        &task_execution_harness_context,
        &task_execution_b.context
    );

    /*
     * Control returns here only from the test-only cleanup
     * continuation after B has become READY and cleanup has become
     * the scheduler's current task.
     */

    if (task_destroy(&task_execution_a) != 0)
    {
        task_test_fail(
            "TASK TERMINATED CLEANUP: FAILED\n"
        );
    }

    /*
     * Task B yielded to the test-only cleanup continuation and is
     * now READY and owned by the runqueue. Its real VMM-backed
     * stack can therefore be reclaimed through task_destroy().
     */
    if (task_execution_b.state != TASK_STATE_READY ||
        !runqueue_contains(&task_execution_b) ||
        scheduler_get_current() !=
            &task_execution_cleanup_task)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP HANDOFF: FAILED\n"
        );
    }

    if (runqueue_remove(&task_execution_b) != 0)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP REMOVE: FAILED\n"
        );
    }

    if (task_destroy(&task_execution_b) != 0)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP DESTROY: FAILED\n"
        );
    }

    if (task_execution_b.state !=
            TASK_STATE_TERMINATED ||
        task_execution_b.kernel_stack_base != 0 ||
        task_execution_b.kernel_stack_top != 0)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP RELEASE: FAILED\n"
        );
    }

    serial_write_string(
        "TASK EXECUTION: VERIFIED\n"
    );

    serial_write_string(
        "TASK ENTRY ARGUMENT: VERIFIED\n"
    );

    serial_write_string(
        "TASK EXIT: VERIFIED\n"
    );

    serial_write_string(
        "TASK EXIT AUTHORITY: VERIFIED\n"
    );

    serial_write_string(
        "TASK TERMINATION DISPATCH: VERIFIED\n"
    );
}

static int task_execution_exit_handler(struct task *task)
{
    if (task == NULL)
        return -1;

    /*
     * task_exit() has already transitioned the task to TERMINATED
     * and invalidated its continuation authority. The scheduler
     * still owns the task as current until this handler performs
     * the dispatch.
     */
    if (task_destroy(task) != 0)
    {
        task_execution_destroy_rejected = 1;
    }
    else
    {
        task_test_fail(
            "TASK CURRENT TERMINATED DESTROY: ACCEPTED\n"
        );
    }

    return scheduler_exit_current(task);
}

static void task_execution_test_a(void *argument)
{
    task_execution_a_reached = 1;

    if ((uint64_t)(uintptr_t)argument !=
        task_execution_expected_argument)
    {
        task_test_fail(
            "TASK ENTRY ARGUMENT: FAILED\n"
        );
    }

    /*
     * Simulate an older interrupt continuation becoming stale while
     * the task is still running. task_exit() must invalidate it.
     */
    task_execution_a.resume_authority =
        TASK_RESUME_INTERRUPT;

    task_execution_a_terminated = 1;
}

static void task_execution_test_b(void *argument)
{
    (void)argument;

    task_execution_b_reached = 1;

    /*
     * Task A returned from its entry with stale INTERRUPT authority.
     * The bootstrap path called task_exit(), so a terminated task
     * must no longer have any resumable continuation authority.
     */
    if (task_execution_a.resume_authority !=
        TASK_RESUME_NONE)
    {
        task_test_fail(
            "TASK EXIT AUTHORITY: FAILED\n"
        );
    }

    /*
     * Task B is still RUNNING and remains the scheduler's current
     * task. Return to the harness without changing scheduler
     * ownership so the primary A -> B termination contract can
     * be asserted independently from the later cleanup path.
     *
     * When the harness restores B's saved context, execution
     * resumes immediately after this switch and B performs the
     * legitimate scheduler-owned cleanup handoff.
     */
    x86_64_context_switch(
        &task_execution_b.context,
        &task_execution_harness_context
    );

    /*
     * B genuinely resumes here only after the harness has verified
     * the primary termination-dispatch state.
     *
     * Construct a test-only cooperative continuation so B can
     * leave RUNNING state through the real scheduler before its
     * VMM-backed stack is reclaimed.
     */
    task_execution_cleanup_task.id = 4;
    task_execution_cleanup_task.state =
        TASK_STATE_READY;
    task_execution_cleanup_task.kernel_stack_base = 0;
    task_execution_cleanup_task.kernel_stack_top = 0;
    task_execution_cleanup_task.address_space = 0;
    task_execution_cleanup_task.process = NULL;
    task_execution_cleanup_task.entry =
        task_execution_cleanup_entry;
    task_execution_cleanup_task.argument = NULL;
    task_execution_cleanup_task.wait_queue = NULL;
    task_execution_cleanup_task.resume_authority =
        TASK_RESUME_CONTEXT;
    task_execution_cleanup_task.preempt_state.frame_address = 0;
    task_execution_cleanup_task.preempt_state.valid = 0;

    task_execution_cleanup_task.context.rbx = 0;
    task_execution_cleanup_task.context.rbp = 0;
    task_execution_cleanup_task.context.r12 = 0;
    task_execution_cleanup_task.context.r13 = 0;
    task_execution_cleanup_task.context.r14 = 0;
    task_execution_cleanup_task.context.r15 = 0;

    uint64_t cleanup_stack_top =
        (uint64_t)(uintptr_t)(
            task_execution_cleanup_stack + 4096
        );

    cleanup_stack_top -= sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)cleanup_stack_top = 0;

    task_execution_cleanup_task.context.rsp =
        cleanup_stack_top;
    task_execution_cleanup_task.context.rip =
        (uint64_t)(uintptr_t)
            task_execution_cleanup_entry;

    if (scheduler_add(
            &task_execution_cleanup_task
        ) != 0)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP ADMISSION: FAILED\n"
        );
    }

    if (scheduler_yield() != 0)
    {
        task_test_fail(
            "TASK EXECUTION CLEANUP YIELD: FAILED\n"
        );
    }

    /*
     * The cleanup continuation owns the return to the harness.
     * B must never resume into this entry during this test.
     */
    task_test_fail(
        "TASK EXECUTION B RESUMED UNEXPECTEDLY\n"
    );
}

static void task_execution_cleanup_entry(void *argument)
{
    (void)argument;

    /*
     * The scheduler has committed this manually constructed
     * continuation as RUNNING. Transfer control back to the
     * already-saved test harness continuation.
     */
    x86_64_context_switch(
        &task_execution_cleanup_task.context,
        &task_execution_harness_context
    );

    task_test_fail(
        "TASK EXECUTION CLEANUP RESUMED UNEXPECTEDLY\n"
    );
}
