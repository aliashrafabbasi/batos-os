#include "lifecycle_tests.h"

#include <stdint.h>
#include <stddef.h>

#include "../lifecycle/lifecycle.h"
#include "../sched/task.h"
#include "../sched/task_registry.h"
#include "../sched/runqueue.h"
#include "../sched/scheduler.h"
#include "../process/process.h"
#include "../process/process_registry.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

static void lifecycle_test_entry(void *argument)
{
    (void)argument;

    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );
    }
}

static void lifecycle_test_fail(const char *message)
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

void lifecycle_tests_run(void)
{
    /*
     * The lifecycle queue intentionally retains these objects after
     * this test function returns, so their storage must outlive
     * the test invocation.
     */
    static struct process process;
    static struct task task;
    uint64_t address_space;

    serial_write_string(
        "\nLIFECYCLE TEST\n"
    );


    if (task_registry_init() != 0)
        lifecycle_test_fail("LIFECYCLE TASK REGISTRY INIT: FAILED\n");

    if (process_registry_init() != 0)
        lifecycle_test_fail("LIFECYCLE PROCESS REGISTRY INIT: FAILED\n");

    if (scheduler_init() != 0)
        lifecycle_test_fail("LIFECYCLE SCHEDULER INIT: FAILED\n");

    address_space = vmm_get_pml4();

    if (address_space == 0)
        lifecycle_test_fail("LIFECYCLE ADDRESS SPACE: FAILED\n");

    if (process_create(&process, 200, address_space) != 0)
        lifecycle_test_fail("LIFECYCLE PROCESS CREATE: FAILED\n");

    if (task_create(
            &task,
            200,
            address_space,
            lifecycle_test_entry,
            NULL
        ) != 0)
    {
        lifecycle_test_fail("LIFECYCLE TASK CREATE: FAILED\n");
    }

    if (task_registry_register(&task) != 0)
        lifecycle_test_fail("LIFECYCLE TASK REGISTER: FAILED\n");

    if (process_attach_task(&process, &task) != 0)
        lifecycle_test_fail("LIFECYCLE PROCESS ATTACH: FAILED\n");

    /*
     * The task must be READY and therefore scheduler-owned before
     * termination can be exercised through the normal lifecycle
     * boundary.
     */
    if (scheduler_add(&task) != 0)
        lifecycle_test_fail("LIFECYCLE SCHEDULER ADD: FAILED\n");

    if (scheduler_start() != 0)
        lifecycle_test_fail("LIFECYCLE SCHEDULER START: FAILED\n");

    /*
     * A RUNNING Task cannot be published because publication
     * requires the TERMINATED lifecycle state.
     */
    if (lifecycle_publish_terminated_task(&task) == 0)
        lifecycle_test_fail("LIFECYCLE RUNNING PUBLISH REJECTION: FAILED\n");

    if (task_exit(&task) != 0)
        lifecycle_test_fail("LIFECYCLE TASK TERMINATION: FAILED\n");

    /*
     * Low-level Task construction is unmanaged by default. A
     * terminated unmanaged Task must therefore not enter the
     * deferred-reclamation lifecycle.
     */
    if (lifecycle_publish_terminated_task(&task) == 0)
        lifecycle_test_fail("LIFECYCLE UNMANAGED PUBLISH REJECTION: FAILED\n");

    task.lifecycle_mode =
        TASK_LIFECYCLE_MANAGED;

    /*
     * Publication is safe while the terminated Task is still
     * scheduler-current because publication does not destroy the
     * execution stack or release scheduler-current ownership.
     */
    if (lifecycle_publish_terminated_task(&task) != 0)
        lifecycle_test_fail("LIFECYCLE CURRENT PUBLISH: FAILED\n");

    if (lifecycle_pending_count() != 1)
        lifecycle_test_fail("LIFECYCLE PENDING COUNT: FAILED\n");

    if (!lifecycle_contains_pending(&task))
        lifecycle_test_fail("LIFECYCLE PENDING MEMBERSHIP: FAILED\n");

    /*
     * The pending Task remains registry- and Process-owned until
     * successful reclamation.
     */
    if (!task_registry_contains(&task))
        lifecycle_test_fail("LIFECYCLE REGISTRY RETENTION: FAILED\n");

    if (!process_contains_task(&process, &task))
        lifecycle_test_fail("LIFECYCLE PROCESS RETENTION: FAILED\n");

    /*
     * Reaping the current terminated Task must remain forbidden.
     */
    if (lifecycle_reap_one() == 0)
        lifecycle_test_fail("LIFECYCLE CURRENT REAP REJECTION: FAILED\n");

    /*
     * Release CPU ownership through a direct scheduler-state reset
     * is intentionally NOT performed here. Lifecycle-1 must not
     * invent a scheduler/reaper execution context.
     *
     * The test therefore only verifies the lifecycle boundary up
     * to publication. Safe reaping will be exercised in the next
     * integration step after terminal scheduler handoff is wired.
     */
    serial_write_string(
        "LIFECYCLE SUBSYSTEM: VERIFIED\n"
    );

    serial_write_string(
        "LIFECYCLE PUBLICATION: VERIFIED\n"
    );

    serial_write_string(
        "LIFECYCLE OWNERSHIP RETENTION: VERIFIED\n"
    );
}
