#include "preempt_tests.h"

#include "../sched/task.h"
#include "../arch/x86_64/sched/preempt.h"
#include "../arch/x86_64/interrupt/irq.h"
#include "../arch/x86_64/cpu/gdt.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>

extern void x86_64_task_bootstrap_trampoline(void);

#define PREEMPT_INITIAL_RFLAGS 0x202ULL

static void preempt_test_entry(void *argument)
{
    (void)argument;
}

static void preempt_test_fail(const char *message)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

void preempt_tests_run(void)
{
    serial_write_string(
        "\nPREEMPTION-1B CONTRACT TEST\n"
    );

    struct task task = {0};

    uint64_t pml4 = vmm_get_pml4();

    if (pml4 == 0)
    {
        preempt_test_fail(
            "PREEMPT TEST: NO ADDRESS SPACE\n"
        );
    }

    if (task_create(
            &task,
            6,
            pml4,
            preempt_test_entry,
            NULL
        ) != 0)
    {
        preempt_test_fail(
            "PREEMPT TASK CREATE: FAILED\n"
        );
    }

    if (!x86_64_preempt_state_is_valid(
            &task.preempt_state
        ))
    {
        preempt_test_fail(
            "PREEMPT STATE: INVALID\n"
        );
    }

    uint64_t expected_bootstrap_rsp =
        task.kernel_stack_top -
        3ULL * sizeof(uint64_t);

    uint64_t expected_frame_address =
        expected_bootstrap_rsp -
        sizeof(struct irq_frame);

    if (task.preempt_state.frame_address !=
        expected_frame_address)
    {
        preempt_test_fail(
            "PREEMPT FRAME ADDRESS: INVALID\n"
        );
    }

    if (expected_frame_address <
            task.kernel_stack_base ||
        expected_frame_address +
            sizeof(struct irq_frame) >
            task.kernel_stack_top)
    {
        preempt_test_fail(
            "PREEMPT FRAME BOUNDS: INVALID\n"
        );
    }

    if ((expected_frame_address & 0xFULL) != 0)
    {
        preempt_test_fail(
            "PREEMPT FRAME ALIGNMENT: INVALID\n"
        );
    }

    struct irq_frame *frame =
        (struct irq_frame *)(uintptr_t)
            task.preempt_state.frame_address;

    /*
     * The first 15 qwords are the architecture-owned
     * synthetic GPR state.
     */
    uint64_t *gpr_words =
        (uint64_t *)(uintptr_t)frame;

    for (uint64_t index = 0;
         index < 15;
         index++)
    {
        if (gpr_words[index] != 0)
        {
            preempt_test_fail(
                "PREEMPT INITIAL GPRS: INVALID\n"
            );
        }
    }

    if (frame->vector != 0)
    {
        preempt_test_fail(
            "PREEMPT VECTOR: INVALID\n"
        );
    }

    if (frame->rip !=
        (uint64_t)(uintptr_t)
            x86_64_task_bootstrap_trampoline)
    {
        preempt_test_fail(
            "PREEMPT RIP: INVALID\n"
        );
    }

    if (frame->cs != GDT_KERNEL_CODE)
    {
        preempt_test_fail(
            "PREEMPT CS: INVALID\n"
        );
    }

    if (frame->rflags != PREEMPT_INITIAL_RFLAGS)
    {
        preempt_test_fail(
            "PREEMPT RFLAGS: INVALID\n"
        );
    }

    /*
     * The preemptive representation must not alter the
     * established cooperative context ABI.
     */
    if (task.context.rsp != expected_bootstrap_rsp ||
        (task.context.rsp & 0xFULL) != 8 ||
        *(uint64_t *)(uintptr_t)
            task.context.rsp != 0 ||
        *(uint64_t *)(uintptr_t)(
            task.context.rsp +
            sizeof(uint64_t)
        ) != (uint64_t)(uintptr_t)&task ||
        task.context.rip !=
            (uint64_t)(uintptr_t)
                x86_64_task_bootstrap_trampoline)
    {
        preempt_test_fail(
            "PREEMPT COOPERATIVE ABI: INVALID\n"
        );
    }

    serial_write_string(
        "PREEMPT STATE: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT FRAME ADDRESS: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT FRAME BOUNDS: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT FRAME ALIGNMENT: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT INITIAL GPRS: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT RETURN FRAME: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPT COOPERATIVE ABI: PRESERVED\n"
    );

    if (task_destroy(&task) != 0)
    {
        preempt_test_fail(
            "PREEMPT TASK DESTROY: FAILED\n"
        );
    }

    if (task.preempt_state.valid != 0 ||
        task.preempt_state.frame_address != 0 ||
        x86_64_preempt_state_is_valid(
            &task.preempt_state
        ))
    {
        preempt_test_fail(
            "PREEMPT DESTROY INVALIDATION: FAILED\n"
        );
    }

    serial_write_string(
        "PREEMPT DESTROY INVALIDATION: VERIFIED\n"
    );

    serial_write_string(
        "PREEMPTION-1B CONTRACT: VERIFIED\n"
    );
}
