#include "runtime_tests.h"

#include "../console/console.h"
#include "../process/process_registry.h"
#include "../runtime/runtime.h"
#include "../runtime/runtime_test.h"
#include "../mm/vmm/vmm.h"
#include "../sched/runqueue.h"
#include "../sched/task_registry.h"

#include <stddef.h>
#include <stdint.h>

#define KERNEL_RUNTIME_TEST_PID       1ULL
#define KERNEL_RUNTIME_TEST_ROOT_TID  1ULL
#define KERNEL_RUNTIME_TEST_REAPER_TID 2ULL

static void runtime_test_fail(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
    {
        __asm__ volatile ("cli\nhlt");
    }
}

void runtime_tests_run(void)
{
    uint64_t kernel_address_space =
        vmm_get_pml4();

    if (kernel_address_space == 0)
    {
        runtime_test_fail(
            "KERNEL RUNTIME FAILURE ATOMICITY: INVALID PML4\n"
        );
    }

    serial_write_string(
        "KERNEL RUNTIME FAILURE ATOMICITY: START\n"
    );

    /*
     * Force failure after the production root execution unit has
     * been published, but before the lifecycle reaper is published.
     *
     * This specifically exercises Runtime-owned composition
     * rollback.
     */
    kernel_runtime_test_fail_reaper_once();

    if (kernel_runtime_init() == 0)
    {
        runtime_test_fail(
            "KERNEL RUNTIME FAILURE ATOMICITY: FAILURE ACCEPTED\n"
        );
    }

    /*
     * The failed initialization must leave every production
     * execution-ownership domain empty.
     */
    if (process_registry_count() != 0)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: PROCESS REGISTRY DIRTY\n"
        );
    }

    if (task_registry_count() != 0)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: TASK REGISTRY DIRTY\n"
        );
    }

    if (runqueue_count() != 0)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: RUNQUEUE DIRTY\n"
        );
    }

    if (vmm_get_pml4() != kernel_address_space)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: KERNEL PML4 CHANGED\n"
        );
    }

    if (process_registry_find(
            KERNEL_RUNTIME_TEST_PID
        ) != NULL)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: PID 1 PUBLISHED\n"
        );
    }

    if (task_registry_find(
            KERNEL_RUNTIME_TEST_ROOT_TID
        ) != NULL)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: TID 1 PUBLISHED\n"
        );
    }

    if (task_registry_find(
            KERNEL_RUNTIME_TEST_REAPER_TID
        ) != NULL)
    {
        runtime_test_fail(
            "KERNEL RUNTIME ROLLBACK: TID 2 PUBLISHED\n"
        );
    }

    serial_write_string(
        "KERNEL RUNTIME FAILURE ATOMICITY: VERIFIED\n"
    );
}
