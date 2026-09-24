#include "runtime.h"
#include "runtime_test.h"

#include "../arch/x86_64/sched/context.h"
#include "../arch/x86_64/sched/preempt.h"
#include "../console/console.h"
#include "../execution/execution.h"
#include "../lifecycle/lifecycle.h"
#include "../arch/x86_64/time/clock_source.h"
#include "../arch/x86_64/time/timer_service.h"
#include "../mm/vmm/vmm.h"
#include "../process/process.h"
#include "../process/process_registry.h"
#include "../sched/scheduler.h"
#include "../sched/task.h"
#include "../sched/task_registry.h"

#include <stdint.h>

#define KERNEL_RUNTIME_PID          1ULL
#define KERNEL_RUNTIME_TASK_TID     1ULL
#define KERNEL_REAPER_TASK_TID      2ULL

/*
 * These identities are deliberately reserved bootstrap identities.
 *
 * They are not a permanent PID/TID allocation policy. A dedicated
 * identity-allocation subsystem will replace these constants before
 * general process creation is introduced.
 */
static struct process kernel_runtime_process;
static struct task kernel_runtime_root_task;
static struct task kernel_runtime_reaper_task;

static struct x86_64_context
    kernel_runtime_bootstrap_context;

static uint8_t kernel_runtime_initialized = 0;
static uint8_t kernel_runtime_root_reaped_reported = 0;

/*
 * Test-only one-shot failure injection at the production runtime
 * composition boundary.
 *
 * This deliberately lives in runtime.c rather than in Execution,
 * Scheduler, Task, or Registry code because the test verifies
 * Runtime's ownership rollback contract, not a lower-layer fault
 * model.
 */
static uint8_t kernel_runtime_fail_reaper_once = 0;

/*
 * Production runtime owns composition rollback for objects that it
 * has successfully published. Each flag means the corresponding
 * execution unit completed its publication transaction and therefore
 * must be destroyed through the execution-layer ownership boundary.
 */
static uint8_t kernel_runtime_root_published = 0;
static uint8_t kernel_runtime_reaper_published = 0;

void kernel_runtime_test_fail_reaper_once(void)
{
    kernel_runtime_fail_reaper_once = 1;
}

static int kernel_runtime_rollback(void)
{
    /*
     * Roll back in reverse publication order.
     *
     * Reaper was published after root, so it must be destroyed first.
     * The execution layer releases Scheduler -> Process membership ->
     * Task Registry -> Task ownership for us.
     */
    if (kernel_runtime_reaper_published)
    {
        if (execution_destroy_task(
                &kernel_runtime_process,
                &kernel_runtime_reaper_task
            ) != 0)
        {
            return -1;
        }

        kernel_runtime_reaper_published = 0;
    }

    /*
     * Root owns the kernel Process lifecycle. Its destruction therefore
     * also terminates and destroys the Process after the Task is gone.
     */
    if (kernel_runtime_root_published)
    {
        if (execution_destroy_kernel_task(
                &kernel_runtime_process,
                &kernel_runtime_root_task
            ) != 0)
        {
            return -2;
        }

        kernel_runtime_root_published = 0;
    }

    kernel_runtime_root_reaped_reported = 0;

    return 0;
}

static void kernel_runtime_halt_failure(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );
    }
}

static void kernel_runtime_root_entry(
    void *argument
)
{
    (void)argument;

    serial_write_string(
        "KERNEL ROOT TASK: RUNNING\n"
    );

    /*
     * Keep the root Task runnable for several timer events.
     *
     * The LAPIC interrupt boundary may preempt this Task while
     * the Task itself makes no scheduler call.
     */
    for (uint64_t tick = 0;
         tick < 4;
         tick++)
    {
        __asm__ volatile ("hlt");
    }

    serial_write_string(
        "KERNEL ROOT TASK: EXITING\n"
    );

    /*
     * Returning transfers terminal execution to the established
     * task_bootstrap_entry() lifecycle boundary.
     */
}

static void kernel_runtime_reaper_entry(
    void *argument
)
{
    (void)argument;

    serial_write_string(
        "KERNEL LIFECYCLE REAPER: RUNNING\n"
    );

    for (;;)
    {
        int result = lifecycle_reap_one();

        if (result < 0)
        {
            kernel_runtime_halt_failure(
                "KERNEL LIFECYCLE REAPER: FAILED\n"
            );
        }

        /*
         * Root Task reclamation is observable only after its Task
         * Registry membership and Process membership have both been
         * released by lifecycle_reap_one().
         */
        if (!kernel_runtime_root_reaped_reported &&
            task_registry_find(
                KERNEL_RUNTIME_TASK_TID
            ) == NULL &&
            process_task_count(
                &kernel_runtime_process
            ) == 1)
        {
            kernel_runtime_root_reaped_reported = 1;

            serial_write_string(
                "KERNEL LIFECYCLE REAPER: ROOT TASK REAPED\n"
            );
        }

        /*
         * Reaper is a persistent kernel worker. HLT releases the
         * CPU between interrupt-driven wakeups without changing
         * scheduler ownership semantics.
         */
        __asm__ volatile ("hlt");
    }
}

int kernel_runtime_init(void)
{
    uint64_t kernel_address_space =
        vmm_get_pml4();

    if (kernel_runtime_initialized)
        return -1;

    if (kernel_address_space == 0)
        return -2;

    /*
     * Timer service must exist before clock-source calibration,
     * because clock_event_notify() feeds timer_service_tick().
     */
    timer_service_init();

    /*
     * Production runtime owns the final system clock-source state.
     *
     * Foundation tests may temporarily manipulate timer hardware,
     * but they are not a prerequisite for production operation.
     * Re-establish the calibrated LAPIC 100 Hz source here before
     * any production Task is allowed to execute with interrupts
     * enabled.
     */
    if (clock_source_init(100) != 0)
        return -3;

    /*
     * Deterministic tests intentionally reinitialize these global
     * ownership domains. Production runtime therefore establishes
     * a fresh authoritative initialization boundary after tests.
     */
    if (process_registry_init() != 0)
        return -4;

    if (task_registry_init() != 0)
        return -5;

    if (scheduler_init() != 0)
        return -6;

    if (lifecycle_init() != 0)
        return -7;

    /*
     * PID 1 becomes the permanent kernel Process identity.
     * TID 1 is its first execution context.
     */
    if (execution_create_kernel_task(
            &kernel_runtime_process,
            &kernel_runtime_root_task,
            KERNEL_RUNTIME_PID,
            KERNEL_RUNTIME_TASK_TID,
            kernel_address_space,
            kernel_runtime_root_entry,
            NULL
        ) != 0)
    {
        return -8;
    }

    kernel_runtime_root_published = 1;

    /*
     * TID 2 joins the same Process and provides the persistent
     * lifecycle execution context.
     *
     * The one-shot failure is consumed exactly at this production
     * composition boundary so the following real initialization
     * attempt is unaffected.
     */
    if (kernel_runtime_fail_reaper_once)
    {
        kernel_runtime_fail_reaper_once = 0;

        if (kernel_runtime_rollback() != 0)
            return -9;

        return -9;
    }

    if (execution_create_task(
            &kernel_runtime_process,
            &kernel_runtime_reaper_task,
            KERNEL_REAPER_TASK_TID,
            kernel_address_space,
            kernel_runtime_reaper_entry,
            NULL
        ) != 0)
    {
        if (kernel_runtime_rollback() != 0)
            return -9;

        return -9;
    }

    kernel_runtime_reaper_published = 1;

    if (kernel_runtime_process.state !=
            PROCESS_STATE_ACTIVE ||
        kernel_runtime_process.address_space !=
            kernel_address_space ||
        process_task_count(
            &kernel_runtime_process
        ) != 2 ||
        kernel_runtime_root_task.state !=
            TASK_STATE_READY ||
        kernel_runtime_reaper_task.state !=
            TASK_STATE_READY)
    {
        if (kernel_runtime_rollback() != 0)
            return -10;

        return -10;
    }

    kernel_runtime_initialized = 1;

    serial_write_string(
        "KERNEL PRODUCTION RUNTIME: INITIALIZED\n"
    );

    return 0;
}

int kernel_runtime_start(void)
{
    if (!kernel_runtime_initialized)
        return -1;

    if (scheduler_start() != 0)
        return -2;

    if (scheduler_get_current() !=
            &kernel_runtime_root_task ||
        kernel_runtime_root_task.state !=
            TASK_STATE_RUNNING)
    {
        return -3;
    }

    serial_write_string(
        "KERNEL PRODUCTION SCHEDULER: START\n"
    );

    /*
     * Preemption authority is enabled only after scheduler_start()
     * has established a real current Task. IF remains disabled until
     * the task bootstrap trampoline executes STI.
     */
    x86_64_preempt_enable();

    /*
     * The kernel boot context is the external first-dispatch owner.
     * scheduler_start() deliberately remains ownership-only.
     */
    x86_64_context_switch(
        &kernel_runtime_bootstrap_context,
        &kernel_runtime_root_task.context
    );

    return -4;
}
