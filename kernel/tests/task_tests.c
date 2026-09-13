#include "task_tests.h"

#include "../sched/task.h"
#include "../mm/pmm/pmm.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

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

void task_tests_run(void)
{
    serial_write_string(
        "\nTASK FOUNDATION TEST\n"
    );

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
        task.kernel_stack_base == 0 ||
        task.kernel_stack_top <=
            task.kernel_stack_base ||
        task.context.rsp !=
            task.kernel_stack_top -
            sizeof(uint64_t) ||
        task.context.rip == 0 ||
        free_before - free_after_create !=
            TASK_KERNEL_STACK_PAGE_COUNT)
    {
        task_test_fail(
            "TASK CREATE: INVALID\n"
        );
    }

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

    if (task_destroy(&task) != 0)
    {
        task_test_fail(
            "TASK DESTROY: FAILED\n"
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
}
